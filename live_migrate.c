#include "ckpt.h"

#define MEMSECTION_NUM 15

//FIXME:
// I am using an array to store received memory section headers.
// This design limits the number of memory section the process can
// have or memory will be wasted.
// A linked list data structure should be used.
mem_section* msection_array[MEMSECTION_NUM];

// socket endpoint of "live_migrate" process
int skt_live_migrate;

int main(int argc, char *argv[])
{
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) == -1){
    exit_with_msg("prctl()");
  }

  assert(argc == 3);
  static uint32_t server_addr;
  server_addr = (uint32_t) atoi(argv[1]);
  static in_port_t server_port;
  server_port  = (in_port_t) atoi(argv[2]);
 
  // read the address and size of the original stack.
  int maps_fd = open("/proc/self/maps", O_RDONLY);
  static char line[128];
  static mem_section msection;
  while (_readline(maps_fd, line) != -1)
  {
    if (is_stack_line(line))
    {
      fill_memsection(&msection, line);
      break;
    }
  }
  if (close(maps_fd) == -1){
    exit_with_msg("close()");
  }
  
  //  move this process's stack.
  static void *new_stack_addr;
  new_stack_addr = (void *)0x6700100; //
  static size_t new_stack_size;
  new_stack_size = 0x6701000-0x6700000;
  if (mmap((void *)0x6700000, new_stack_size, PROT_READ|PROT_WRITE, 
      MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED, -1, 0) == (void *)-1)
  {
    exit_with_msg("mmap()");
  }
 
  asm volatile ("mov %0,%%rsp;" : : "g" (new_stack_addr) : "memory");
  restore_memory();

  static void *old_stack_addr;
  old_stack_addr = msection.address;
  static size_t old_stack_size;
  old_stack_size = msection.size;
  // unmap the old_stack region
  if (munmap(old_stack_addr, old_stack_size) == -1){
    exit_with_msg("munmap()");
  } 

  // the address of the server to connect to.
  static struct sockaddr_in serverAddr;
  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = server_addr;
  serverAddr.sin_port = server_port;
  // socket endpoint of "live_migrate" process
  if ((skt_live_migrate = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
    exit_with_msg("socket()");
  }
  // connect to the server("ckpt") process.
  if (connect(skt_live_migrate, (struct sockaddr *) &serverAddr,
                                 (socklen_t) sizeof(serverAddr)) == -1){
    exit_with_msg("connect()");
  }
  
  // request memory mappings
  static enum command cmd = MEM_MAP;
  if (write(skt_live_migrate, &cmd, sizeof(cmd)) == -1){
    exit_with_msg("write()");
  }

  receive_ckpt_image(skt_live_migrate);

  // receive the context
  memset(&context, 0, sizeof(context));
  if (read(skt_live_migrate, &context, sizeof(context)) == -1){
    exit_with_msg("read()");
  }
  
  // -- then restart --
  // first install a PAGE FAULT handler: using sigaction
  static struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_flags = SA_SIGINFO|SA_ONSTACK;
  action.sa_sigaction = segfault_handler;
  sigemptyset(&action.sa_mask);

  if (sigaction(SIGSEGV, &action, NULL) == -1){
    exit_with_msg("sigaction()");
  }

  static int *p;
  read(skt_live_migrate, &p, sizeof(p));
  *p = 0;
  //printf("%d\n", *p); 

  if (setcontext(&context) == -1){
    exit_with_msg("setcontext()");
  }
}  
  
void
receive_ckpt_image(int fd){
  // receive the memory sections and map them
  static int prot = PROT_NONE;
  static int flags = MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS;
  static mem_section msection;
//  static int i; //index into the memory section array
//  i = 0;
  memset(&msection, 0, sizeof(msection));
  if (read(fd, &msection, sizeof(msection)) == -1){
    exit_with_msg("read()");
  }
  // allocate memory(heap) to the ith memory section header.
  // And keep the section header in the array.
//  msection_array[i] = (mem_section *) malloc(sizeof(mem_section));
//  msection_array[i]->readable = msection.readable;
//  msection_array[i]->writable = msection.writable;
//  msection_array[i]->executable = msection.executable;
//  msection_array[i]->address = msection.address;
//  msection_array[i]->size = msection.size;
//  msection_array[i]->is_stack = msection.is_stack;
//  msection_array[i]->is_last_section = msection.is_last_section;
//  i++;
  while (msection.is_last_section == FALSE){
    if (msection.is_stack == TRUE){
      flags = flags|MAP_GROWSDOWN;
      prot = PROT_READ|PROT_WRITE;
    }
    if (mmap(msection.address, msection.size, prot, flags, 
                                      -1, 0) == (void *)-1){
      exit_with_msg("mmap()");
    }
    if (msection.is_stack == TRUE){
      if (read(fd, msection.address, msection.size) == -1){
        exit_with_msg("read()");
      }
      prot = PROT_NONE; // reset the protection to NONE.
    } 
  
    // read the next memory header section.
    memset(&msection, 0, sizeof(msection));
    if (read(fd, &msection, sizeof(msection)) == -1){
      exit_with_msg("read()");
    }
    // allocate memory(heap) to the ith memory section header.
    // And keep the section header in the array.
//    msection_array[i] = (mem_section *) malloc(sizeof(mem_section));
//    msection_array[i]->readable = msection.readable;
//    msection_array[i]->writable = msection.writable;
//    msection_array[i]->executable = msection.executable;
//    msection_array[i]->address = msection.address;
//    msection_array[i]->size = msection.size;
//    msection_array[i]->is_stack = msection.is_stack;
//    msection_array[i]->is_last_section = msection.is_last_section;
//    i++;
  }
}


void* hexstring_to_int(char *hexstring){
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

int _readline(int fd, char *line){
  char c;
  char *line_p = line;
  int cnt;
  while ((cnt = read(fd, &c, 1)) != 0){
    *line_p++ = c;
    if (c == '\n')
      break;
  }

  if (cnt = 0){ // end of file read() return value = 0.
    return -1;
  }

  return 0;
}

int is_stack_line(char *line){
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

void exit_with_msg(const char *msg){
  perror(msg);
  exit(EXIT_FAILURE);
}

void restore_memory(void){return;}


void segfault_handler(int signum, siginfo_t *siginfo, void *ucp){
  // send PAG_FAULT cmd to the server
  enum command cmd = PAGE_FAULT;
  if (write(skt_live_migrate, &cmd, sizeof(cmd)) == -1){
    exit_with_msg("write()");
  }
  //-- send the page address --
  // get which address segfaulted 
  void *addr = (void *) siginfo->si_addr;
  void *VPaddr = addr_to_VPaddr(addr);
  // change the permission in the corresponding mem region.
  //FIXME: 
  // for now I am giving all permissions, should give the right
  // permissions.
  // make sure the page is mapped.
  int prot = PROT_NONE;
  int flags = MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS;  
  if (mmap(VPaddr, PGSIZE, prot, flags, -1, 0) == (void *)-1){
    exit_with_msg("mmap()"); 
  }
  if (mprotect(VPaddr, PGSIZE, PROT_WRITE|PROT_READ|PROT_EXEC) == -1){
    exit_with_msg("mprotect()");
  }

  if (write(skt_live_migrate, &VPaddr, sizeof(VPaddr)) == -1){
    exit_with_msg("write()");
  }
  //-- receive the page -- 
  // read data in memory

  if (read(skt_live_migrate, VPaddr, PGSIZE) == -1){
    exit_with_msg("read()");
  }
  // the execution continues where it segfaulted, it
  // reexecutes the same instruction but it won't segfault this time.
}

void *addr_to_VPaddr(void *addr){
  void *VPaddr = (void *) ((long long unsigned)addr >> SHIFT);
  VPaddr = (void *) ((long long unsigned)VPaddr << SHIFT);
  
  return VPaddr;
}
