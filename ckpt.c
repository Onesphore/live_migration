#include "ckpt.h"

__attribute__((constructor)) void before_main(void){
  if (signal(SIGNUM, checkpoint) == SIG_ERR){
    exit_with_msg("signal()");
  }
}

int x = 9;
int *xp = &x;
void 
checkpoint(int signal_USR2){
  pid_t pid = getpid();
//  pid_t ppid = getppid();
//  static pid_t product;
//  product  = pid * ppid; // just in case live_migrate might have the same pid
                         // but it is very less likely it will have the same ppid.

  memset(&context, 0, sizeof(context));
  if (getcontext(&context) == -1){
    exit_with_msg("getcontext()");
  }
  if (x == 0){
    printf("hello there: ckpt.c!\n");
    printf("%d\n", x);
  }
  if (x == 9){
  //if (is_ckpt == 31)

    int libckpt_host_fd;
    if ((libckpt_host_fd = open("libckpt-host.txt", O_RDONLY)) == -1){
      exit_with_msg("open()");
    }
  
    char libckpt_host[256];
    if (read(libckpt_host_fd, libckpt_host, sizeof(libckpt_host)) == -1){
      exit_with_msg("read()");
    }
    if (close(libckpt_host_fd) == -1){
      exit_with_msg("close()");
    }
    
    // this process will act as a server: --client-server model--
    // address of server
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    serverAddr.sin_port = htons(0); //will get a random port when we bind
    // create socket endpoint of server
    int listenfd;
    if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
      exit_with_msg("socket()");
    }
    // bind to the server address
    if (bind(listenfd, (struct sockaddr *) &serverAddr, 
                                            sizeof(serverAddr)) == -1){
      exit_with_msg("bind()");
    }
    // Discover what listener_port was assigned
    socklen_t addr_len = (socklen_t) sizeof(serverAddr);
    if (getsockname(listenfd, (struct sockaddr *) &serverAddr,
                                                     &addr_len) == -1){
      exit_with_msg("getsockname()");
    }
    in_port_t listener_port = serverAddr.sin_port;
    // mark the socket endpoint of server passive.
    listen(listenfd, 10);

    pid_t pid = fork();
    if (pid == -1){
      exit_with_msg("fork()");
    }

    if (pid == 0){ // the child process
      char addr[64];
      sprintf(addr, "%u", serverAddr.sin_addr.s_addr);
      char port[64];
      sprintf(port, "%u", serverAddr.sin_port);
      execl("live_migrate", "live_migrate", addr, port, NULL);
    } 
//    int libckpt 
//    char ssh_cmd[256];
//    sprintf(ssh_cmd, "ssh %s %s/live_migrate %s %d&",
//          libckpt_host, getcwd(), gethostname(), listenr_port);
//    system(ssh_cmd);

    // accept a connection request from "live_migrate" process
    int connfd;
    if ((connfd = accept(listenfd, (struct sockaddr *) &serverAddr,
                                            &addr_len)) == -1){
      exit_with_msg("accept()");
    }
    // receive a command from the client.
    enum command cmd;
    if (read(connfd, &cmd, sizeof(cmd)) == -1){
      exit_with_msg("read()");
    }
    if (cmd == MEM_MAP){
      send_ckpt_image(connfd);
    }
    // send the context.
    if (write(connfd, &context, sizeof(context)) == -1){
      exit_with_msg("write()");
    }  
    if (write(connfd, &xp, sizeof(xp)) == -1){
      exit_with_msg("write()");
    }

    void *pageAddr;
//    size_t nread;
//    memset(&cmd, 0, sizeof(cmd));
//    while ((nread == read(connfd, &cmd, sizeof(cmd))) > 0){
//      if (cmd != PAGE_FAULT){
//        printf("should respect the protocol, bye!\n");
//        exit(EXIT_FAILURE);
//      }
//      memset(&pageAddr, 0, sizeof(pageAddr));
//      if (read(connfd, &pageAddr, sizeof(pageAddr)) == -1){
//        exit_with_msg("read()");
//      }
//      if (write(connfd, pageAddr, PGSIZE) == -1){
//        exit_with_msg("write()");
//      }
//      memset(&cmd, 0, sizeof(cmd));
//    }    
//
//    if (nread == 0){
//      printf("live_migrate disconnected!\n");
//      fflush(stdout);
//    }
//    if (nread == -1){
//      exit_with_msg("read()");
//    }
    int nread, nwrite;
    while (1){ // wait for PAGE_FAULT requests.
      memset(&cmd, 0, sizeof(cmd));
      if ((nread = read(connfd, &cmd, sizeof(cmd))) == -1){
        exit_with_msg("read()");
      }
      if (nread == 0){
        continue;
      }
      if (cmd == PAGE_FAULT){
        //the read the page address
        memset(&pageAddr, 0, sizeof(pageAddr));
        if (read(connfd, &pageAddr, sizeof(pageAddr)) == -1){
          exit_with_msg("read()");
        }
        // send PAGE sized data in that address.
        if (write(connfd, pageAddr, PGSIZE) == -1){
          exit_with_msg("write()");
        }

      }else{
           ;
       //  close(connfd);
       //  exit(EXIT_SUCCESS);
         //printf("should respect the protocol\n");
         //exit(EXIT_FAILURE);//FIXME: better measure should be taken.
      }
    }
  }
}

void
send_ckpt_image(int connfd){
  // open the current process("hello") mapping image
  int maps_fd;
  if ((maps_fd = open("/proc/self/maps", O_RDONLY)) == -1)
  {
    exit_with_msg("open()");
  }

  // read the current process("hello") mapped memory sections
  // and send them to "live_migrate" process.
  char line[128];
  mem_section msection;
  int section_nbr = 0;
  void *access_limit = (void *)(long long)0xf000000000000000;

  while (_readline(maps_fd, line) != -1)
  {
    // check if it's any of the region we don't need to write
    // to the ckpt image file.
    if (is_vvar_line(line)){
      continue;
    }
    if (is_vdso_line(line)){
      continue;
    }
    if (is_vsyscall_line(line)){
      continue;
    }      

    fill_memsection(&msection, line);
    if (msection.address > access_limit){
      continue;
    }
    // send the section header
    msection.is_last_section = FALSE;
    if (write(connfd, &msection, sizeof(msection)) == -1){
      exit_with_msg("write()");
    } 
    if (msection.is_stack == TRUE){
      if (write(connfd, msection.address, msection.size) == -1){
        exit_with_msg("write()");
      }
    }
  
  }

  msection.is_last_section = TRUE;
  if (write(connfd, &msection, sizeof(msection)) == -1){
    exit_with_msg("write()");
  }
}


int 
_readline(int fd, char *line){
  memset(line, 0, 128);

  char c;
  char *line_p = line;
  int cnt;
  while ((cnt = read(fd, &c, 1)) !=0)
  {
    *line_p++ = c;
    if (c == '\n'){
      break;
    }
  }
  
  if (cnt == 0){ // end of file read() return value = 0.
    return -1;
  }

  return 0;
}


void 
fill_memsection(mem_section *msection_p, char *line){
  memset(msection_p, 0, sizeof(mem_section));
  char *line_p = line;
  //1. get the address where this memory section begins
  void *addr_begin, *addr_end;
  char hex_str[17];
  char *hex_str_p = hex_str;

  memset(hex_str, 0, 17);
  while (*line_p != '-'){
    *hex_str_p++ = *line_p++;
  }

  addr_begin = hexstring_to_int(hex_str);

  //  . get the address where this memory section ends
  hex_str_p = hex_str;
  memset(hex_str, 0, 17);
  line_p++; // to get past "-"
  while (*line_p != ' '){
    *hex_str_p++ = *line_p++;
  }
  addr_end = hexstring_to_int(hex_str);
 
  msection_p->address = addr_begin;
  msection_p->size = (size_t) (addr_end - addr_begin);
 
  line_p++; // get past " "

  //3. get access mode: r/w/x
  msection_p->readable = *line_p++;
  msection_p->writable = *line_p++;
  msection_p->executable = *line_p++;

  //4. check if this memory section is a stack region
  while (*line_p != 's' && *line_p != '\n'){
    line_p++;
  }
  if (*line_p == '\n'){
    msection_p->is_stack = FALSE;
    return;
  }  


  if (*line_p == 's'){
    char stack_str[6];
    char *stack_str_p = stack_str;
    int i;
    for (i=0; i<5; ++i){
      *stack_str_p++ = *line_p++;
    }
    *stack_str_p = 0;
    if (!strcmp(stack_str, "stack"))
      msection_p->is_stack = TRUE;
    
    return;
  }

  msection_p->is_stack = FALSE;
  return;
}

void* 
hexstring_to_int(char *hexstring){
  void* ret_val=0;

  int base = 16; 
  unsigned long long base_to_exp;

  size_t len = strlen(hexstring);
  int exp;
  for (exp=len-1; exp>=0; --exp){
    if (exp == len-1){
      base_to_exp = 1;
    }         
    else{
      base_to_exp *= 16;
    }

    if (hexstring[exp] >= 'a' && hexstring[exp] <= 'f'){
      ret_val += (hexstring[exp] - ('a'-10)) * base_to_exp;
    } else {
      ret_val += (hexstring[exp] -'0') * base_to_exp;
    }
  }
  return ret_val;
}

int 
is_stack_line(char *line){
  char *line_p = line;
  while (*line_p != 's' && *line_p != '\n'){
    line_p++;
  }
  if (*line_p == '\n')
    return 0;

  if (*line_p == 's'){
    char stack_str[6];
    char *stack_str_p = stack_str;
    int i;
    for (i=0; i<5; ++i){
      *stack_str_p++ = *line_p++;
    }
    *stack_str_p = 0;
    if (!strcmp(stack_str, "stack"))
      return 1;
  }

  return 0;
}

int 
is_vvar_line(char *line){
  char *line_p = line;
  while (*line_p != 'v' && *line_p != '\n'){
    line_p++;
  }
  if (*line_p == '\n')
    return 0;

  if (*line_p == 'v'){
    char vvar_str[5];
    char *vvar_str_p = vvar_str;
    int i;
    for (i=0; i<4; ++i){
      *vvar_str_p++ = *line_p++;
    }
    *vvar_str_p = 0;
    if (!strcmp(vvar_str, "vvar"))
      return 1;
  }

  return 0;
}

int 
is_vdso_line(char *line){
  char *line_p = line;
  while (*line_p != 'v' && *line_p != '\n'){
    line_p++;
  }
  if (*line_p == '\n')
    return 0;

  if (*line_p == 'v'){
    char vdso_str[5];
    char *vdso_str_p = vdso_str;
    int i;
    for (i=0; i<4; ++i){
      *vdso_str_p++ = *line_p++;
    }
    *vdso_str_p = 0;
    if (!strcmp(vdso_str, "vdso"))
      return 1;
  }

  return 0;
}

int 
is_vsyscall_line(char *line){
  char *line_p = line;
  while (*line_p != 'v' && *line_p != '\n'){
    line_p++;
  }
  if (*line_p == '\n')
    return 0;

  if (*line_p == 'v'){
    char vsyscall_str[9];
    char *vsyscall_str_p = vsyscall_str;
    int i;
    for (i=0; i<8; ++i){
      *vsyscall_str_p++ = *line_p++;
    }
    *vsyscall_str_p = 0;
    if (!strcmp(vsyscall_str, "vsyscall"))
      return 1;
  }

  return 0;
}


void exit_with_msg(const char *msg){
  perror(msg);
  exit(EXIT_FAILURE);
}
