#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // your code here.  you should write the secret to fd 2 using write
  
// (e.g., write(2, secret, 8) pFaAgIeL
  char secret[8];
  char* end = sbrk(64 * PGSIZE);
  for(int i = 0; i < 64; i++){
    char* page = end + PGSIZE * i;
    for(int j = 0; j < 8; j++){
        secret[j] = page[j+32];
    }
    
    if(secret[0] != 0 && i == 16){
      write(2, secret, 8);
      //printf("in page %d found %s\n", i, secret);
    }
  }
  
  exit(1);
}
