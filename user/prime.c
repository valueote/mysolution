#include "kernel/types.h"
#include "user/user.h"

void primes(int) __attribute__((noreturn));

void
primes(int fd)
{
  int buf;

  if(!read(fd, &buf, sizeof(int))){
    close(fd);
    exit(0);
  }
  int p = buf;
  printf("prime %d\n", p);
  int pipeline[2];
  pipe(pipeline);

  if(fork() == 0){
    close(pipeline[1]);
    close(fd);
    primes(pipeline[0]);
    close(pipeline[0]);
  }else{
    close(pipeline[0]);

    while(read(fd, &buf, sizeof(int)) > 0){
      if(buf % p != 0){
        write(pipeline[1], &buf, sizeof(int));
      }
    }  
    close(fd);
    close(pipeline[1]);
    wait(0);
  }  
  exit(0);
}

int main()
{
  /*
  p = get a number from left neighbor
  print p
  loop:
    n = get a number from left neighbor
    if (p does not divide n)
        send n to right neighbor
   */
  int pipeline[2];
  pipe(pipeline);
  int buf;

  if(fork() == 0){
    close(pipeline[1]);
    primes(pipeline[0]);
    close(pipeline[0]);
    wait(0);
  }else {
    close(pipeline[0]);
    for(int i = 2; i < 281; i++){
      buf = i;
      write(pipeline[1], &buf, sizeof(int));
    }
    close(pipeline[1]);
    wait(0);
  }
  exit(0);
}
