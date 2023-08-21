#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char **argv)
{
  if(argc < 2) {
    exit();
  }

  int pid, priority;
  pid = atoi(argv[1]);
  priority = atoi(argv[2]);

  set_priority(pid, priority); // sysproc.c 에 정의됨
  printf(1,"unlocked\n");
  exit();
}
