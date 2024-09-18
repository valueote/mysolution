#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void find(char* path, char* filename){
  
  int fd;
  
  struct stat st;
  char* cur = ".";
  char* up = "..";
    
  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    exit(1);
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    exit(1);
  }

  switch(st.type){
  case T_DEVICE:
  case T_FILE:
    if(!strcmp(filename, path)){
      printf("%s\n", path);
    }
    break;
  case T_DIR:
    if(!strcmp(filename, path)){
      printf("%s\n", path);
    }

    if(strcmp(path, cur) && strcmp(path, up)){
      printf("%s", "not finished\n");
    }
    break;
  }
}

int
main(int argc, char* argv[]){
  if(argc < 3){
    fprintf(2,"Useage: find path filename\n");
    exit(0);
  }

  find(argv[1], argv[2]);
  exit(0);
}
