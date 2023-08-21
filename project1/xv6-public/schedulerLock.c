#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char **argv)
{
  if(argc < 1) {
    exit();
  }

  if(strcmp(argv[1], "\"2019038513\"")!= 0) {
    // wrong command
    // scheduler_lock_unlock_fail();
    printf(1,"wrong\n");
    exit();
    return 0;
  }

  __asm__("int $129");
  printf(1,"locked\n");
  return 0;
}
