#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void find(char* path, char* filename){
  char buf[512],*p;
  int fd;
  struct dirent de;
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
      printf("%s\n", buf);
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;

      if(stat(buf, &st) < 0){
        printf("find: cannot stat %s\n", buf);
        continue;
      }
      if(!strcmp(p, filename))
        printf("%s\n", buf);
      
      if(st.type == T_DIR && strcmp(up, p) && strcmp(cur, p))
        find(buf,filename);
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
