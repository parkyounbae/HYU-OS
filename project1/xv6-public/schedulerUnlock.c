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
    exit();
  }

  scheduler_unlock(); // sysproc.c 에 정의됨
  printf(1,"unlocked\n");
  exit();
}
