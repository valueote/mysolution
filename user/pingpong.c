#include "kernel/types.h"
#include "user/user.h"

int
main()
{
  int p[2];
  pipe(p);
  char buf[1];
  
  if(fork() == 0){
    int cpid = getpid();

    read(p[0], buf, 1);
    printf("%d: received ping\n", cpid);

    write(p[1], buf, 1);
    
    exit(0);
  }else{
    int ppid = getpid();

    write(p[1], "p", 1);
    wait(0);

    read(p[0], buf, 1);
    printf("%d: received pong\n", ppid);

    close(p[0]);
    close(p[1]);
    exit(0);
  }
  
}
