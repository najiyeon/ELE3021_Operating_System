// Create a zombie process that
// must be reparented at exit.

#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  if(fork() > 0)
    sleep(2000);  // Let child exit before parent.
  exit();
}
