#include "kernel/types.h"
#include "user/user.h"

void primes(int) __attribute__((noreturn));

void
primes(int fd)
{
  int p;

  if(read(fd, &p, sizeof(int)) <= 0){
    close(fd);
    exit(0);
  }
  
  printf("prime %d\n", p);

  int pipeline[2];
  pipe(pipeline);

  if(fork() == 0){
    close(fd);
    close(pipeline[1]);
    primes(pipeline[0]);
    close(pipeline[0]);
  }else{
    close(pipeline[0]);
    int n;
    while(read(fd, &n, sizeof(int)) > 0){
      if(n % p != 0){
        write(pipeline[1], &n, sizeof(int));
      }
    }  
    close(fd);
    close(pipeline[1]);
    wait(0);
  }  
  exit(0);
}

int
main()
{
  int pipeline[2];
  pipe(pipeline);

  if(fork() == 0){
    close(pipeline[1]);
    primes(pipeline[0]);
    close(pipeline[0]);
    }else {
    close(pipeline[0]);
    for(int i = 2; i < 281; i++){
      write(pipeline[1], &i, sizeof(int));
    }
    close(pipeline[1]);
    wait(0);
  }
  exit(0);
}

