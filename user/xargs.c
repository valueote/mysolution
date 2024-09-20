#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

void runcmd(char *args[])
{
  if(exec(args[0], args) < 0){
    fprintf(2,"exec: failed to execute\n");
    exit(1);
  }
  exit(0);
}

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2,"Usage: xargs command\n");
    exit(1);
  }

  char* args[MAXARG];
  char buf;
  char arg[128];
  char* p = arg;

  for(int i = 1; i < argc; i++){
    args[i-1] = argv[i];
  }  
  argc -= 1;
  
  while(read(0, &buf, sizeof(char))){
    if(buf == '\n'){
      args[argc] = arg;
      if(fork() == 0){
        runcmd(args);
      }
      p = arg;
      memset(arg, 0, sizeof(arg));
    }else{
      *p++ = buf;
    }
  } 
  wait(0);
  exit(0);
}
