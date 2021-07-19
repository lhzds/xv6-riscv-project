#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

char* 
fmtname(char *path) {
  char* t;
  for (t = path + strlen(path); t >= path && *t != '/'; --t)
    ;
  return ++t;
}

void 
find(char* path, char* fname) {
  char buf[512], *p;
  int fd;
  struct stat st;
  struct dirent de;
  
  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }
  
  switch (st.type) {
    case T_FILE:
      if (strcmp(fmtname(path), fname) == 0) {
        printf("%s\n", path);
      }
      break;
    
    case T_DIR:
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(path);
      *p++ = '/';
      while (read(fd, &de, sizeof (de)) == sizeof (de)) {
        if (de.inum == 0 || strcmp(".", de.name) == 0 || strcmp("..", de.name) == 0) {
          continue;
        }
        strcpy(p, de.name);
        find(buf, fname);
      }
      break;
  }
  close(fd);
}

int
main (int argc, char* argv[]) {
   if (argc != 3) {
     fprintf(2, "usage: find path filename\n");
     exit(1);
   }
   find(argv[1], argv[2]);
   exit(0);
}

