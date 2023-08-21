#include "types.h"
#include "stat.h"
#include "user.h"

int gcnt;

void nop(){ }

void*
racingthreadmain(void *arg)
{
  int tid = (int) arg;
  int i;
  //int j;
  int tmp;
  printf(1, "thread_start!!!!!\n");
  for (i = 0; i < 10000000; i++){
    tmp = gcnt;
    tmp++;
    nop();
    gcnt = tmp;
  }
  printf(1, "thread_end!!!!!\n");
  thread_exit((void *)(tid+1));
  printf(1, "thread_exit!!!!!\n");
  return 0;
}

int main(int argc, char *argv[])
{
  thread_t threads[5];
  int i;
  void *retval;
  gcnt = 0;
  
  for (i = 0; i < 5; i++){
    if (thread_create(&threads[i], racingthreadmain, (void*)i) != 0){
      printf(1, "panic at thread_create\n");
      return  -1;
    }
	printf(1, "thread_create %d\n", i);
  }
  for (i = 0; i < 5; i++){
    if (thread_join(threads[i], &retval) != 0 || (int)retval != i+1){
      printf(1, "panic at thread_join\n");
      return -1;
    }
	printf(1, "thread_join %d\n", i);
  }
  printf(1,"%d\n", gcnt);
  exit();
};
