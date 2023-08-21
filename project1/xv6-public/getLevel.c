#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char **argv)
{
  get_level(); // sysproc.c 에 정의됨
  printf(1,"getLevel\n");
  exit();
}
