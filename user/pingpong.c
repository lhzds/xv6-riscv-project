#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char* argv[]) {
  char buf1[1], buf2[1];
  int p1[2], p2[2];
  pipe(p1);
  pipe(p2);
  
  if (fork() == 0) {
    close(p1[1]);
    close(p2[0]);
    read(p1[0], buf1, 1);
    close(p1[0]);
    printf("%d: received ping\n", getpid());
    write(p2[1], (char)0, 1);
    close(p2[1]);
  } else {
    close(p1[0]);
    close(p2[1]);
    write(p1[1], (char)0, 1);
    close(p1[1]);
    wait(0);
    read(p2[0], buf2, 1);
    printf("%d: received pong\n", getpid());
    close(p2[0]);
  }
  
  exit(0);
}
  
