#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
  int ticks = uptime() + 5000;
  while (uptime() < ticks);

  printf(1,"end\n");
  exit();
};
