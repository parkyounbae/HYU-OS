# ELE3021_project02_12300_2019038513

# Design

> 명세에서 요구하는 조건에 대해서 어떻게 구현할 계획인지, 어떤 자료구조와 알고리즘이
필요한지, 자신만의 디자인을 서술합니다.
> 

### Pmanager

먼저 pmanager.c 에서는 사용자의 입력을 받고 입력받은 명령을 수행합니다. 수행 가능한 명령어에는 list, execute, kill, memlim, exit 가 있습니다. 먼저 list는 현재 ptable에 RUNNABLE 혹은 RUNNING인 프로세스들의 정보를 출력해 줍니다. 이때 쓰레드의 상태를 출력하지 않도록 합니다. 원하는 정보를 출력하기 위해 proc.c에 manager_list라는 시스템 콜을 추가해 줍니다. kill + pid 명령어의 경우에는 해당 pid를 가진 프로레스를 종료 시킵니다. 기존의 kill 시스템 콜을 호출합니다. execute + path + stacksize 명령어의 경우에는 기존의 exec와 비슷하지만 스택페이지 의 개수를 사용자가 원하는 개수로 실행시킬 수 있도록 할당 부분을 수정한 exec2를 통해 프로세스를 실행시킵니다. 이때 프로그램의 실행 이후 종료를 기다리지 않고 pmanager가 다시 실행되어야 합니다. 이를 위해 sh의 작동 방식을 비슷하게 구현하였습니다. memlim + pid + limit 명령어의 경우 해당 pid를 가진 프로세스의 메모리 제한을 설정해줍니다. 이를 위해 proc.c에 프로세스 정보에 접근하는 시스템 콜을 만들어 주었습니다. 마지막으로 exit 명령어를 받으면 pmanager를 종료시킵니다. 

### LWP

먼저 기본적인 생각으로는 본프로세스를 실행시키고 그 프로세스 위에다가 스레드용 스택+가드 페이지를 쌓아가는 것 입니다. 이후 쓰레드용 프로세스를 할당해주고 본프로세스와 공우하는 부분에 해당하는 것은 exec함수의 일부분을 통해 공유하도록 하고 각각 가지고 있어야 하는 스택+가드용 페이지는 본프로세스에서 할당 받은 곳을 가리키게 합니다. 이를 통해 간접적인 쓰레드를 구현해 주고 이렇게 생성한 쓰레드를 ptable에 넣어 일반적인 프로세스들과 같이 스케쥴링 되도록 합니다. 가장 많이 신경썼던 부분은 할당시의 메모리 주소값입니다. 기존의 프로세스의 맨위에 스택을 쌓는 방식이기 때문에 해당 주소값을 쓰레드 마다 잘 지정해 주어야 합니다. 

![Untitled](ELE3021_project02_12300_2019038513%20dcd9a0954bc4430f9ff1c619cda9a42b/Untitled.jpeg)

또한 이러한 구조에 걸맞게 fork, kill, exit 등등 함수들을 수정해야 했다. 각각 쓰레드 인지 아닌지 판별하고 쓰레드 일 경우 알맞게 처리 되도록 하게 하였다. 

# Implement

### Pmanager

명령어를 입력 받고 파싱하여 올바른 동작을 하게 만들었습니다.  출력을 할 때 쓰레드인 경우에는 출력을 하지 않도록 하였습니다.

- list

```c
if(command[0]=='l') {
            if(command[1]!='i' || command[2]!='s' || command[3]!='t') {
                return -1;
            }
            // 해당 명령이 list인 경우 manager_list 시스템 콜을 호출합니다. 
            manager_list();
            continue;
        }
```

쓰레드를 제외하고 러너블, 러닝, 슬리핑 상태일 프로세스들에 대한 정보를 출력합니다. 

```c
void manager_list(void) {
  // 현재 실행중인 프로세스들의 정보를 출력한다.
  // 이름, pid, 스택용 페이지 개수, 할당받은 메모리의 크기, 메모리 리미트
  // cprintf("list sys\n");
  struct proc *p;
  for(p = ptable.proc ; p<&ptable.proc[NPROC] ; p++) {
    if(p->state== RUNNABLE && p->is_thread == 0) {
      cprintf("p_name: %s, pid: %d, page_size: %d, allocated memory: %d, limit: %d\n", p->name, p->pid, p->stack_size, p->sz, p->limit);
    } else if (p->state== RUNNING && p->is_thread == 0) {
      cprintf("p_name: %s, pid: %d, page_size: %d, allocated memory: %d, limit: %d\n", p->name, p->pid, p->stack_size, p->sz, p->limit);
    } else if (p->state== SLEEPING && p->is_thread == 0) {
      cprintf("p_name: %s, pid: %d, page_size: %d, allocated memory: %d, limit: %d\n", p->name, p->pid, p->stack_size, p->sz, p->limit);
    }
  }
}
```

- kill

사용자 프로그램으로부터 pid를 입력받고 시스템 콜은 기존의 xv6의 kill 시스템 콜을 그대로 사용합니다.

```c
else if(command[0]=='k') {
            // kill + pid
            // 해당 명령어가 kill 일 경우
            if(command[1]!='i' || command[2]!='l' || command[3]!='l') {
                return -1;
            }
            int pid = 0;
            int index = 5;
            // pid를 파싱합니다. 
            // 한개의 숫자씩 입력 받고 10씩 곱해주어 입력 받은 값을 완성시킵니다. 
            for(int i = index ; command[i]!= '\n' ; i++) {
                pid = pid*10;
                pid = pid + (command[i]-'0');
            }

            if(pid <1 || pid >1000000000) {
                // 입력 받은 수가 범위를 넘어서면 -1을 반환합니다. 
                return -1;
            }
            // kill시스템 콜을 호출합니다.
            int result;
            result = kill(pid);
            // 반환 값이 0이면 성공이고 -1이면 실패
            if(result == 0) {
                printf(1, "kill success\n");
            } else {
                printf(1, "kill fail\n");
            }
        }
```

- execute

사용자로부터 명령을 입력받고 파싱하며 exec2 함수를 실행시킵니다. 

```c
else if(command[0]=='e') {
            // execute + path + stacksize
            // 입력 받은 명령이 execute인 경우
            if(command[1]!='x' || command[2]!='e' || command[3]!='c' ||command[4]!='u' || command[5]!='t' || command[6]!='e') {
                printf(1, "execute : fail");
                return -1;
            }
            char path[50];
            int stacksize=0;
            char *argv[10];

            int path_size = 0; // 파싱을 위한 입력받은 주소의 길이를 기록합니다. 
            // 명렬어가 7글자이고 공백하나 있어서 8부터 시작
            for(int i = 8 ; command[i]!= ' ' ; i++) {
                path[path_size] = command[i];
                path_size++;
                // commnad에 있던 프로그램 이름을 path로 넘깁니다.
            }
            path[path_size] = '\0'; // 마지막에 널을 넣어주어 끝을 알림

            // pid 파싱과 같은 방식으로 높은 자리수 부터 한자리 숫자씩 받아 10씩 곱하면서 진행
            for(int i = path_size+9 ; command[i]!= '\n' ; i++) {
                stacksize = stacksize * 10;
                stacksize = stacksize + (command[i]-'0');
            }
            if(stacksize <1 || stacksize >1000000000) {
                // 스택 사이즈의 허용된 범위를 넘어서면 -1 반환
                printf(1, "execute : fail");
                return -1;
            }
            // exec2에 인자로 넘겨줄 것들
            argv[0] = path;
            argv[1] = 0;

            // 벡그라운드에서 프로그램이 돌아가고 pmanager는 계속해서 돌아가야 하기 때문에 
            // 포크를 한 뒤 자식은 실행하려고 했던 프로그램을 수행하고 
            // 부모는 다시 반복문의 맨 위로 올라가서 사용자의 입력을 받을 수 있도록 한다.
            int pid = 0;
            pid = fork();
            if(pid==0) {
                // exec2 시스템콜을 호출하여 입력받은 프로그램을 수행한다. 
                exec2(argv[0], argv, stacksize);
                exit();
            } else if(pid>0) {
                //wait();
                // pmanager 프로그램은 기다리지 않고 다시 위로 올라가서 사용자의 입력을 기다린다. 
                continue;
            } else {
                // 살패시 메시지 출력
                printf(1, "execute : fail\n");
            }
        }
```

exec2함수는 기존의 exec를 거의 그대로 가져와 사용하였는데 차이점은 메모리를 할당하는 범위에서 차이가 납니다. 기존에는 가드용페이지1 + 스택용페이지1 이였다면 지금은 가드용페이지1 + 설정한스택용페이지크기 로 메로리를 할당해 줍니다. 또한  추가된 proc 구조체의 요소들을 초기화 하는 부분에서 차이가 납니다. 스택사이즈를 입력받은 스택 사이즈로 바꿔줄 뿐만 아니라 쓰레드 관련 여러 요소들을 초기 값으로 설정해줍니다. 

```c
int
exec2(char *path, char **argv, int stacksize)
{
    if(stacksize<1 || stacksize>100) {
	    // 올바르지 않은 범위
	    cprintf("exec: wrong stack size\n");
	    return -1;
    }

		...

		// Allocate two pages at the next page boundary.
	  // Make the first inaccessible.  Use the second as the user stack.
	  sz = PGROUNDUP(sz);
	  if((sz = allocuvm(pgdir, sz, sz + (stacksize+1)*PGSIZE)) == 0)
	  // 내가 원하는 크기 + 1 만큼 일단 할당 받아보자~
	  // 기존의 sz에서 새롭게 할당 받은 크기만큼 받고 새로운 sz 반환
	    goto bad;
	  clearpteu(pgdir, (char*)(sz - (stacksize+1)*PGSIZE)); // 원하는 스택 개수 + 가드용 페이지 할당
	  sp = sz;

		...

// Commit to the user image.
  // 여러가지 초기 정보들을 초기화해준다.
  oldpgdir = curproc->pgdir; 
  curproc->pgdir = pgdir; // 테이블 복사
  curproc->sz = sz;
  curproc->tf->eip = elf.entry;  // main
  curproc->tf->esp = sp;

  curproc->limit = 0;
  curproc->stack_size = stacksize; // 입력받은 스택 사이즈로 
  for(int i=0 ; i<64 ; i++) {
    curproc->thread_context[i] = 0;
  }
  curproc->stack_start = sz;
  curproc->is_thread = 0;
  curproc->num_thread = 0;
  curproc->max_thread = 64;
  curproc->tid = 0;
  curproc->thread_top = -1; // 처음에는 쓰레드가 없기 때문에 -1로 설정

		...
}
```

- memlim

명령어를 입력받고 memlim 시스템 콜을 호출합니다. 

```c
else if(command[0]=='m') {
            // memlim + pid + limit 해당 명령어를 입력받았을 때
            if(command[1]!='e' || command[2]!='m' || command[3]!='l' ||command[4]!='i' || command[5]!='m') {
                return -1;
            }
            int pid=0;
            int limit=0;
            int index = 7; // 파싱을 위한 인덱스

            while(command[index]!=' ') {
                // 높은 자리수 부터 하나씩 입력받고 10씩 곱해줘서 원하는 pid값을 뽑아준다. 
                pid = pid*10;
                pid = pid + (command[index]-'0');
                index++;
            }
            index++;
            // 제한 값도 pid와 같은 방식으로 뽑아준다. 
            while(command[index]!='\n') {
                index = index*10;
                index = index + (command[index]-'0');
                index++;
            }
            // memlim 시스템콜을 호출하고 반환값을 저장한다. 
            int result;
            result = memlim(pid, limit);
            if(result == 0) {
                // 반환값이 0이면 성공메시지, -1이면 실패 메시지를 출력한다. 
                printf(1, "memlim success\n");
            } else {
                printf(1, "memlim fail\n");
            }
        }
```

메인 쓰레드가 아니고 pid가 같은 프로세스를 찾았다면 이후에는 입력받은 리미트 값을 검사합니다. 이미 할당받은 크기보다 제한값이 작거나 입력받은 값이 음수인 경우에는 -1을 반환합니다. 또한 이 값을 이용하여 최대로 할당할 수 있는 쓰레드의 수를 구해줍니다 .

```c
int setmemorylimit(int pid, int limit) {
  // pid에 해당하는 프로세스에 대해 할당 받을 수 있는 메모리의 최대치를 설정해줌
  struct proc *p;
  for(p = ptable.proc ; p<&ptable.proc[NPROC] ; p++) {
    if(p->pid == pid && p->is_thread==0) {
      goto found;
    }
  }

  return -1;

  found:

  if(p->sz  >limit || limit<0) {
// 쓰레드가 포함된 프로세스의 크기보다 제한값이 작거나 입력받은 값이 음수일때
    // cprintf("memory wrong limit err\n");
    return -1;
  }

  acquire(&ptable.lock);
  p->limit = limit; // 프로세스의 리미트 값을 설정해주고
  p->max_thread = (limit - p->sz)/(PGSIZE*(p->stack_size+1)) + p->num_thread;
  // (리미트 - 현재 할당중인 크기)/스택+가드용 크기 를 해주면 남은 공간에 추가로 생성할수 있는 스레드의 수를
  // 구할 수 있다. 거기다가 현재 쓰레드의 수를 더해주면 전체 최대 쓰레드 수를 알 수 있다. 
  release(&ptable.lock);

  return 0;
}
```

- exit

```c
else if(command[0]=='e') {
            // exit
            break;
        }
```

### LWP

- 구조체 변경

lwp를 위해 기존의 proc 구조체를 수정하였다. 

→ thread_context : 64개의 배열을 생성하고 초기값은 0으로 채워둔다. 이때 스택이 쌓이는 인덱스마다 1로 표시해 둔다. 반대로 쓰레드가 종료되면 해당하는 배열의 값을 0으로 바꿔준다. 이를 통해 빈 공간이 생기면 그곳에 새로운 쓰레드를 할당할 수 있어 메모리 누수를 막을 수 있다. 

→ stack_start : 메인 프로세스인 경우 자신의 스택 위가 stack_start 로 지정된다. 이 위에부터 쓰레드의 스택이 쌓이기 시작한다. 쓰레드인 경우 메인 프로세서에서의 해당 쓰레드 스택의 시작점을 나타낸다. 

→ thread_top : 해당 프로세서가 생성된 이후 최대로 쓰레드가 생성된 인덱스를 기록한다. 구조상 한번 늘어난 sz를 줄이지 않는다. 이는 한번 할당된 구역이지만 현재 인덱스에서 존재하는 최대값보다는 크고 thread_top보다 작은 경우에 새로운 공간을 할당하지 않아도 되기 때문에 이를 구현하기 위해 사용된다. (쓰레드 1,2,3 생성 → 2,3 종료 → 이후에 다시 2 실행할때 thread_index가 현재 있는 최대 1 보다 크기 때문에 새롭게 공간을 할당하여 sz가 늘어날 수 있었지만 thread_top을 사용하여 2<3 이기 때문에 새로운 공간을 할당하지 않아도 됨을 알 수 있다. 

→ real_parent : 기존의 프로세스일 경우엔 real_parent = parent 이다. 하지만 쓰레드인 경우에는 parent는 메인 프로세스를 가리키고 real_parent 는 메인 프로세스의 parent를 가리킨다. 

```cpp
	int limit; // 메모리 제한
  int stack_size;

  int thread_context[64]; // 스레드의 상황을 표시해주는 배열 쓰레드가 있으면 1, 없으면 0이다. 이것을 이용해 메모리 할당 위치를 결정한다. 
  void* thread_ret; // 반환 값
  uint stack_start; // 스택이 시작하는 위치를 기록함
  int thread_top; // 지금까지 제일 높았던 쓰레드 인덱스
  int is_thread; // 쓰레드의 구성원이면 1, 아니면 0
  int num_thread; // 현재 lwp 개수
  int max_thread; // 메모리제한이 걸리면 최대로 생성할 수 있는 쓰레드의 수가 변경될 수 있다. 
  int tid; // 현재 쓰레드의 tid
  int thread_index; // 쓰레드가 메인 프로세스의 어디에 위치하는지

  struct proc* real_parent; // 쓰레드라면 메인쓰레드의 부모를 가리키게 된다.
```

- thread_create

가장 먼저 할 일은 쓰레드를 생성하려는 메인 프로세스의 현재 상태를 확인하는 겁니다. 시작하자 마자 두개의 루프를 도는데 첫 번째로는 현대 프로세스의 쓰레드 인덱스 중에서 가장 위에 있는 쓰레드의 인덱스가 몇인지 확인하는 것 입니다. 이는 이 프로세스가 쓰레드로 인해 자치하는 메모리의 최댓값을 알기 위해 수행합니다. 두번째 루프는 빈 공간을 찾는 과정입니다. 배열중 0이 있다면 그 공간은 빈 공간이므로 그곳에 쓰레드를 넣게 됩니다. 그 공간을 1로 바꿔주고 이후 작업으로 넘어갑니다. 

빈 공간을 찾았으므로 먼저 쓰레드의 생성 가능 여부를 조사합니다. 해당 프로세스에 리미트 값이 걸려있고 이미 할당받은 공간 밖에 쓰레드를 생성해야 해 추가로 메모리를 할당받아야 하는 상황이라면 추가로 할당받을 공간과 제한값을 비교해 제한값이 더 작다면 -1을 반환합니다. 

exec의 일부분을 이용해 값을 복사해 줍니다. 

allocproc으로 ptable의 구조체를 할당 받습니다. 이후 쓰레드에 여러가지 정보들을 설정해주고 메인프로세스에 추가로 공간을 더 할당해야 하는 상황이라면 메인 프로세스의 sz를 늘려줍니다. thread_proc의 stack_start는 메인의 stack_stack(쓰레드가 없다는 가정하의 메인 프로스의 sz와 동일) 위에 쓰레드를 얹기 때문에 거기에 쓰레드크기*쓰레드인덱스 를 더해준 값으로 stack start 값을 지정해준다. 그 값을 이용해 쓰레드를 위한 공간을 할당해 줍니다 (stack_start ~ stack_start+thread_size)

fork의 일부분을 이용합니다. 

이후 프로세스 구조체의 정보들을 채워 넣습니다. 상태를 러너블로 바꾸어 스케쥴링 과정해서 수행될 수 있도록 합니다. 

```c
int thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg) {
  // 쓰레드를 생성한다. 
  // 쓰레드 id, 쓰레드가 시작할 지점, 시작하는 함수에 전달 할 인자
  // 성공시 0, 실패시 -1 반환
  // 쓰레드를 생성하고 해당 프로세스의 위에서 페이지를 찾으면서 빈 곳에 스택용페이지+가드용페이지 이렇게 일단 할당해줌
  // fork와 비슷한 과정을 통해 내용을 복사하자
  
  struct proc* thread_proc;
  struct proc* p = myproc();
  uint sp;

  int thread_index = 0; // 몇번째 인덱스에 사용가능한 쓰레드가 있는지 체크

  int max_thread_index = -1; // 제일 상단에 있는 쓰레드가 몇번째 인지 확인
  for(int i=0 ; i<64 ; i++) {
    if(p->thread_context[i] == 1) {
      max_thread_index = i;
    }
  }

  for(int i=0 ; i<p->max_thread ; i++) {
    if(p->thread_context[i]==0) {
      // 0으로 쓰레드가 없는 빈 공간임을 발견했으면 해당 인덱스를 저장하고 1로 바꿔준다. 이후 found로 이동
      thread_index = i;
      p->thread_context[i]=1;
      // cprintf("found %d\n", i);
      goto found;
    }
  }
  cprintf("error1\n");
  // 쓰레드 배열에 사용 가능한 곳이 없으므로 -1 리턴
  return -1;

  found:

  // 해당 프로세스에 제한 값이 있는 경우애는 추가로 할당될 메모리를 포함한 프로레스의 크기보다 제한값이 작다면 더이상
  // 프로세스를 할당할 수 없는 것 이므로 -1을 반환하고 끝낸다. 
  if(p->limit != 0 && max_thread_index < thread_index && p->thread_top < thread_index) {
    if(p->limit < p->sz + (uint)((thread_index-max_thread_index)*(1+p->stack_size)*PGSIZE)) {
      cprintf("over limit\n");
      p->thread_context[thread_index]=0;
      return -1;
    }
  }
// 찾았으므로 일단 이 프로세스 구조체를 ptable에 등록해준다.
  if((thread_proc = allocproc())==0) {
    // ptable이 가득 찼으면 바꿨던 칸을 다시 0으로 바꿔주고 -1 리턴
    p->thread_context[thread_index]=0;
    cprintf("error2\n");
    return -1;
  }

  acquire(&ptable.lock);

  // exec.c 파일의 일부분을 사용하여 공유하는 부분을 설정해준다. 
  thread_proc->parent = p; // 쓰레드의 부모를 p로 설정
  *thread_proc->tf = *p->tf;
  thread_proc->pid = p->pid;
  thread_proc->is_thread = 1; // 쓰레드 임을 표시
  thread_proc->stack_start = p->stack_start + thread_index*(1+p->stack_size)*PGSIZE;
  // 스택의 시작부분을 기록함 -> 기존의 sz + 현재 쓰레드의 인덱스*(가드용1 + 스택용)*페이지크기

  thread_proc->real_parent = p->real_parent; // 진짜 부모를 설정(쓰레드->메인쓰레드의부모, 일반->부모)

  // 현재 할당된 쓰레드의 수 보다 더 높은곳에 쓰레드를 생성해야하는 경우
  if(max_thread_index < thread_index && thread_index > p->thread_top) {
    // 최댓값을 갱신해준다.
    p->thread_top = thread_index;
    p->sz += (uint)((thread_index-max_thread_index)*(1+p->stack_size)*PGSIZE); // 메인 프로세스 위에 쓰레드를 얹는것 이므로 메인 프로세스의 sz를 늘려준다.
  }

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  // 스택용 페이지를 원래 프로세스의 알맞은 위치에 공간을 할당해주고 쓰레드 프로세스의 주소값을 원래 프로세스의 
  // 할당받은 주소로 설정해준다. 
  if((thread_proc->sz = allocuvm(p->pgdir,thread_proc->stack_start , thread_proc->stack_start + (1+p->stack_size)*PGSIZE)) == 0) {
    p->thread_context[thread_index]=0;
    release(&ptable.lock);
    cprintf("error3\n");
    return -1;
  }

clearpteu(p->pgdir, (char*)(thread_proc->sz - (1+p->stack_size)*PGSIZE));
  // cprintf("alloc : %d %d\n", thread_proc->stack_start , thread_proc->stack_start + (1+p->stack_size)*PGSIZE);

  // 총 크기를 일단 sp로 설정해두고
  release(&ptable.lock);

  sp = thread_proc->sz - 4; // argument 공간 확보
  *((uint*)sp) = (uint)arg; // argument
  sp -= 4;
  *((uint*)sp) = 0xffffffff; // fake return PC

  thread_proc->tf->eip = (uint)start_routine;
  thread_proc->tf->esp = sp;

  // fork 부분 활용
  for(int i=0; i<NOFILE; i++)
      if(p->ofile[i])
    thread_proc->ofile[i] = filedup(p->ofile[i]);
  thread_proc->cwd = idup(p->cwd);

  safestrcpy(thread_proc->name, p->name, sizeof(p->name));
  
  acquire(&ptable.lock);
  thread_proc->pgdir = p->pgdir; // page table 복사
  thread_proc->state = RUNNABLE;
  thread_proc->tid = nexttid++;
  thread_proc->thread_index = thread_index;
  *thread = thread_proc->tid;
  p->thread_context[thread_index]=1;
  p->num_thread++;
  cprintf("%d\n", p->sz);
  release(&ptable.lock);
  // cprintf("complete celate\n");
  return 0;
}
```

- thread_join

기존의 wait 함수를 활용하였습니다. 해당 쓰레드 번호를 찾고 부모가 호출한 프로세스와 같다면 해당 프로세스가 종료될 때 까지 기다리거나 이미 종료된 상태라면 자원을 회수해 줍니다. 부모의 페이지에 할당된 자원을 회수해줍니다. (deallocuvm)

```c
void thread_join(thread_t thread, void **retval) {
  struct proc *curproc = myproc();
  struct proc *p;

  if(curproc->is_thread == 1) {
    return;
  }

  // 해당하는 쓰레드가 종료할때까지 기다린다. 
  acquire(&ptable.lock);
  for(;;) {
    for(p=ptable.proc; p<&ptable.proc[NPROC]; p++) {
      if(p->tid == thread && p->is_thread == 1 && p->parent!=curproc) {
        // 쓰레드 번호가 같은데 부모가 내가 아니라면... 빠져나오기
        release(&ptable.lock);
        return;
      }
      if(p->tid == thread && p->is_thread == 1 && p->state == ZOMBIE) {
        // 쓰레드 번호 같은데 좀비 상태라면 -> 자원을 회수하고 정보들을 초기화 시킨다. 
        *retval = p->thread_ret;
        p->parent->num_thread--; // 부모의 쓰레드 개수 줄이기
        *retval = p->thread_ret;
		    p->thread_ret = 0;

        curproc->thread_context[p->thread_index] = 0;

        kfree(p->kstack);
        p->kstack = 0;
        p->pid = 0;
        p->tid = 0;
        p->parent = 0;
        p->real_parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        deallocuvm(p->pgdir, p->sz, p->stack_start);
        release(&ptable.lock);
        return;
      }
    }

    // No point waiting if we don't have any children.
    if(curproc->killed){
      release(&ptable.lock);
      return;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }

}
```

- thread_exit

기존의 exit 함수를 활용하였습니다. 쓰레드가 죽는 정보를 부모에게 반영해주었습니다. 

```c
void thread_exit(void *retval) {
  struct proc *curproc = myproc();
  int fd;

  // Close all open files.
  for(fd=0; fd<NOFILE; fd++) {
    if(curproc->ofile[fd]) {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();

  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Pass abandoned children to init.
  // 쓰레드가 죽는거니까 정보를 반영해준다. 
  curproc->thread_ret = retval;
  curproc->parent->num_thread--;
  curproc->parent->thread_context[curproc->thread_index] = 0;

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
```

- growproc

메모리를 추가할당 할 때에 쓰레드의 경우에는 메인 프로세스의 공간에 추가할당을 받아야 하기 때문에 쓰레드인 경우 메인의 메모리를 추가할당 하도록 코드를 추가하였습니다. growproc을 사용하는 sysproc.c의 sys_sbrk에서도 addr에 저장하는 sz 값을 쓰레드인 경우 메인 프로세스의 sz를 할당해 주어야 하기 때문에 이 부분도 수정해 준다.

```c
int
growproc(int n)
{
  ...

  if(curproc->is_thread == 1) {
    p = curproc->parent;
    sz = p->sz;
    if(n > 0){
      if((sz = allocuvm(p->pgdir, sz, sz + n)) == 0)
        return -1;
    } else if(n < 0){
      if((sz = deallocuvm(p->pgdir, sz, sz + n)) == 0)
        return -1;
    }
    
    p->sz = sz;
    switchuvm(curproc);
    return 0;
  }

  ...
}
```

```c
int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;

  if(myproc()->is_thread == 1) {
    addr = myproc()->parent->sz;
  } else {
    addr = myproc()->sz;
  }
  
  if(growproc(n) < 0)
    return -1;
  return addr;
}
```

- fork

기존의 fork 부분에서 쓰레드 일 경우 복사해야할 메모리의 주소가 다르기 때문에 해당 부분을 수정해 주었습니다.

쓰레드의 경우 메인프로세스의 현재 쓰레드에 해당하는 인데스 부분의 메모리를 복사해 주어야 하기 때문에 parent→stack_start ~ parent→stact_start + thread_size 까지 복사를 하도록 합니다. 

```c
int
fork(void)
{
  ...

  // Copy process state from proc.
  // 포크 시에 기존 프로세스의 맨 마지막 쓰레드를 위한 스택 위에 메모리를 할당해 주어야 한다.
  // 이를 위해 쓰레드 인 경우 맨 위의 인덱스를 가져오고 해당하는 메모리 값을 기준으로 할당해 준다. 

  if(curproc->is_thread == 1) {
    int thread_max_index = 0;
    for(int i=0 ; i<64 ; i++) {
      if(curproc->parent->thread_context[i] == 1) {
        thread_max_index = i+1;
      }
    }

    // (가드용페이지1 + 지정해준스택용페이지)*현재 존재하는쓰레드의맨위인덱스*페이지크기
    if((np->pgdir = copyuvm(curproc->pgdir, curproc->parent->stack_start+(1+curproc->parent->stack_size)*thread_max_index*PGSIZE)) == 0){
      kfree(np->kstack);
      np->kstack = 0;
      np->state = UNUSED;
      return -1;
    }

  } else {
    if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
      kfree(np->kstack);
      np->kstack = 0;
      np->state = UNUSED;
      return -1;
    }
  }

  ...
}
```

- exit

기존의 exit에 쓰레드들을 종료시키는 부분을 추가해 주었습니다. 

```c
void
exit(void)
{
  ...

  acquire(&ptable.lock);
  if(curproc->num_thread != 0 || curproc->is_thread == 1) {
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if(p->pid == curproc->pid && curproc != p) { 
        // 쓰레드가 있다면 모든 쓰레드들을 종료시켜주는 과정이다. 
        p->thread_ret = 0;
        kfree(p->kstack);
        p->kstack = 0;
        p->pid = 0;
        p->tid = 0;
        
        p->parent = 0;
        p->real_parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        p->is_thread = 0;
        p->num_thread = 0;
        p->tid = 0;
        p->thread_index = 0;

        deallocuvm(p->pgdir, p->sz, p->stack_start); // 값들을 다 초기화시켜주고 메모리 할당 해제
        
      }
    }
  } 
  release(&ptable.lock);

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->real_parent); // 잠들어있던 진짜 부모를 깨운다. (그냥 부모는 메인쓰레드를 가리킴)

  ...
}
```

- exec

기존의 exec에서 쓰레드들을 죽이는 함수를 추가해 주었습니다. 쓰레드가 있거나 쓰레드에서 exec를 호출하는 경우에는 쓰레드들을 다 종료시키고 메인 프로세스만 남게 됩니다. 해당 메인 프로세스에서 exec로 실행할 프로그램을 실행하게 됩니다. 

```c
int
exec(char *path, char **argv)
{

...

	// 일단 먼저 패칭하고 해당 프로세스의 스레드들을 다 죽인다. 
  if(curproc->is_thread == 1 || curproc->num_thread != 0) {
    all_kill_thread(curproc);
  }

...

}
```

```c
void all_kill_thread(struct proc* curproc) {
  // exec 에서 해당 프로세스의 쓰레드를 모두 죽이는 함수이다 .
  struct proc* p;
  acquire(&ptable.lock);
  for(p=ptable.proc; p<&ptable.proc[NPROC]; p++) {
    if(p->is_thread == 1 && p->pid==curproc->pid) {
      p->state = ZOMBIE;
    }
  }
  release(&ptable.lock);
}
```

# Result

- thread_exec, thread_exit, thread_kill

![Untitled](ELE3021_project02_12300_2019038513%20dcd9a0954bc4430f9ff1c619cda9a42b/Untitled.png)

- thread_test

![Untitled](ELE3021_project02_12300_2019038513%20dcd9a0954bc4430f9ff1c619cda9a42b/Untitled%201.png)

# Trouble Shooting

1. 아직은 부족한 이해도

전반적으로 시스템을 이해하기는 하였으나 아직은 완벽하게 구조를 이해하지는 못하였다. thread_create 부분 같은 경우 fork와 exec를 일부 차용하여 코드를 작성하였는데 오류가 나면 한줄씩 디버깅 해보며 문제가 되는 부분을 지운 부분 이라던가 growproc을 수정하고 sys_sbrk 를수정할때 addr의 역할을 잘 모른 채 메인을 키우는 것으로 바꾸었으니 여기도 수정해야겠다 라는 생각을 한 등 완벽한 이해를 통한 코드 작성 이라기 보단 감각적인 코드 작성이 일부분 포함되어 있던거 같다. 이러한 부분들이 모여 proc 구조체에 굳이 필요하지 않은 요소들을 추가한건 아닌가, 일부 코드에 굳이 필요없는 라인을 추가 한건 아닌가 하는 찝찝함이 조금 남아 있다. 

1. 최대 64개 까지만 생성이 가능하다.

현재 구조상으로는 쓰레드를 ptable에 올리기 때문에 (쓰레드 수 + 프로세스 수)를 합쳐 최대 64개까지만 등록이 가능하다. 이후에 프로세스 구조체에 쓰레드 배열을 생성하고 스케쥴러에서 쓰레드가 있으면 실행하는 방법을 생각했으나 스케쥴러를 건들여야 하는 부담감이 너무 커 포기했다. 

1. 허점이 많음

현재 구조상으로는 허점이 너무 많은거 같다. 일단 만약 쓰레드를 끝까지 생성후 마지막에 있는거 빼고 다 종료한다고 해도 여전히 그 프로세스는 마지막 번째에 대한 쓰레드를 유지해야 하기 때문에 sz값이 최대로 올라가 있을 것이다. 이를 해결하기 위해서 중간의 쓰레드가 종료된다면 정렬을 해야하는데 이는 너무 어려울거 같아 포기하였다. 이 외에도 여러가지 코너케이스에 대한 점검과 대비를 많이 못한거 같다.
