#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void sieve(int prime, int fd); 

int
main (int arc, char* argv[]) {
  int p[2];
  pipe(p);
  if (fork() == 0) {
    close(p[1]);
    sieve(2, p[0]);
    close(p[0]);
  } else {
    close(p[0]);
    for (int i = 3; i <= 35; ++i) {
      write(p[1], (char*)&i, sizeof (int));
    }
    close(p[1]);
  }
  wait(0);
  exit(0);
}

void
sieve(int prime, int fd) {
  printf("prime %d\n", prime);
  int n, p[2];
  int maked = 0;
  while (read(fd, (char*)&n, sizeof (int)) > 0) {
    if (n % prime != 0) {
      if (!maked) {
        maked = 1;
        pipe(p);
        if (fork() == 0) {
          close(p[1]);
          sieve(n, p[0]);
          close(p[0]);
          wait(0);
          exit(0);
        }
      } else {
        close(p[0]);
        write(p[1], (char*)&n, sizeof (int));
      }
    } 
  }
  if (maked) {
    close(p[1]);
  }
}
