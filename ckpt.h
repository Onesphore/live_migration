#ifndef CKPT_H
#define CKPT_H
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>
#include <sys/prctl.h>

#define SIGNUM SIGUSR2
#define PGSIZE sysconf(_SC_PAGESIZE)
#define SHIFT (int)(log(PGSIZE)/log(2))

#define SERVER_IP "127.0.0.1" // local host

// ----  Global variables ---- //
ucontext_t context; // the variable to store the context of the user program

// the structure used to store useful info
// about mapped memory sections.
enum boolean {FALSE, TRUE};
typedef struct
{
  char readable;
  char writable;
  char executable;

  void *address;              // the address where memory starts.
  size_t size;                // the size of this memory section.

  enum boolean is_stack;      // is this mem section a stack region
  enum boolean is_last_section;  // will let "live_migrate" know when to stop reading memory sections.
} mem_section;

enum command {MEM_MAP = 3, PAGE_FAULT = 7};

// ---- function synopses ---- //
// this function will be run before the main function
// of hello.
// It will install a signal handler. Once the signal is
// received then "hello" will be checkpointed.
__attribute__((constructor)) void before_main(void);
void checkpoint(int);
int _readline(int, char*);
void fill_memsection(mem_section*, char*);
void* hexstring_to_int(char*);
int is_stack_line(char*);
int is_vvar_line(char*);
int is_vdso_line(char*);
int is_vsyscall_line(char*);
void exit_with_msg(const char*);
void send_ckpt_image(int);
void receive_ckpt_image(int);
void restore_memory(void);
void segfault_handler(int, siginfo_t *, void *);
void* addr_to_VPaddr(void *);
#endif
