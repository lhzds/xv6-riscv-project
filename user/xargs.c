#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(2, "usage: args | xargs cmd args\n");
    exit(1);
  }
  char buf[1024];
  char *args[MAXARG];
  
  --argc;
  ++argv;
  for (int i = 0; i < argc; ++i) {
    args[i] = argv[i];
  }
  
  int new_argc = argc, i = 0;
  while (read(0, buf + i, 1)) {
    if (i == 0) {
      args[new_argc++] = buf + i;
      
    } else if (buf[i - 1] == ' ' ) {
      args[new_argc++] = buf + i;
      args[i - 1] = 0;
      
    } else if (buf[i] == '\n') {
      buf[i] = 0;
      args[new_argc] = 0;
      
      if (fork() == 0) {
        exec(argv[0], args);
        exit(0);
      } else {
        wait(0);
        i = -1;
        new_argc = argc;
      }
    }
    ++i;
  }
  
  exit(0);
}
