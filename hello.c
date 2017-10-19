#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]){
   int cnt = 0;
   while (1){
    printf("%d.. ", cnt++);
    fflush(stdout);
    sleep(2);
   }
  
  exit(EXIT_SUCCESS);
}
