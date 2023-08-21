# ELE3021_project01_12300_2019038513

# Design

> 명세에서 요구하는 조건에 대해서 어떻게 구현할 계획인지, 어떤 자료구조와 알고리즘이
필요한지, 자신만의 디자인을 서술합니다.
> 

### 프로세스 관리 구조

기존의 프로세스 구조체의 정보들로는 MLFQ를 구현하기엔 무리가 있어 보였습니다. 때문에 프로세스 구조체에 추가적으로 고려할 변수들을 추가해주었습니다. 추가된 정보들은 프로세스 할당시에 기본 값으로 초기화 되고 스케쥴려, setPriority, boosting 등등 함수들에 의해 변경되며 스케쥴링에 필요한 정보들을 담게 됩니다. 어떤 변수를 추가할지 고민 한 결과 스케쥴러에 제공하는 정보가 프로세스들을 순서대로 진행하는데 필요한 정보들을 줘야겠다고 생각했습니다. 따라서 현재 레벨의 정보를 담고 있는 레벨 변수, 현재 우선순위를 담고 있는 우선순위 변수, 현재 프로세스의 순서 척도를 나타내는 tick정보를 추가해 주었습니다. 또한 현재 프로세스의 남은 퀀텀을 나타내는 퀀텀 변수를 추가해 주었습니다. 

### 스케쥴링

이 프로젝트에서 신경 쓴 부분은 같은 레벨(레벨2의 경우에는 같은 우선순위)에 있을 때 “순서대로” 프로세스가 진행이 된다는 부분이였습니다. 그렇기에 어떤 것을 기준으로 프로세스를 선택할지에 대하여 고민을 한 결과 **레벨→(우선순위)→기록된 global ticks → pid** 순으로 고려를 하여 실행할 프로세스를 결정을 하였습니다. 

먼저 프로세스 테이블을 순회하여 실행 가능한 프로세스 들 중 각각 레벨의 분포를 파악하고 level0에 프로세스가 존재한다면 level0, 없다면 level1, level1에도 없다면 level2를 선택하게 하였습니다. 

이후 각 레벨에서 실행될 프로세스를 선택하는 기준은 저장된 tick을 기준으로 선택을 하였습니다. 이때 프로세스의 저장된 tick은 처음엔 프로세스가 할당 받았을 때의 글로벌 틱을 저장하지만 해당 레벨에서 1tick 실행 될 때마다 현재 글로벌 틱으로 업데이트 해주어 해당 레벨의 맨 뒤로 갈 수 있게 하였습니다. 이는 레벨 별로 유지되어 priority_boosting이 실행되기 전까지 순서가 유지됩니다. 부스팅 이후는 같은 레벨에선 pid기준으로 실행되기 때문에 프로세스가 생성된 순으로부터 시작합니다. 

저장된 tick이 동일한 경우도 고려했습니다. 이는 priority boosting이 시행되었을 때를 대비하여 고려를 한 것입니다. priority boosting이 시행되면 모든 저장된 tick은 초기화 되기 때문에 더이상 비교를 할 수 없기 때문입니다. 이때 pid가 작은 순으로 프로세스를 선택하게 하여 FCFS를 유지할 수 있게 하였습니다. 또한 실행하던 큐가 변경되었다가 다시 돌아왔을 때 실행하던 것을 실행하는 것이 아닌 본래의 들어온 순서대로 실행을 해주기 위해 해당 레벨의 birth_tick을 0으로 초기화 하는 상황에서도 FCFS를 유지할 수 있게 하였습니다. 

### Scheduler Lock / Unlock

해당 함수에서는 간단하게 암호를 확인하고 전역변수 lock_mode를 0또는 1로 바꾸는 정도로만 작동하도록 만들었습니다. 해당 함수에서 프로세스를 직접 처리하기엔 lock관련 혹시모를 오류가 있을거 같았고 모든 스케쥴링은 스케쥴러 함수 내에서 행하는게 좋다고 판단했기 때문입니다. 

스케쥴러 함수 내에서 for문을 돌면서 가장 먼저 확인하는 것이 lock_mode가 1인지 아닌지를 확인하게 하고 lock_mode가 1이라면 독점적으로 해당 프로세스를 조건에 맞게 실행시키도록 하였습니다. 

### 순서를 지키기 위해 (Stride 방식 피하기)

상위 레벨에 프로세스가 들어와 그 레벨을 처리하고 나서 다시 돌아왔을때 실행하던 프로세스부터 실행하는 것이 아닌 처음의 순서인 들어온 순서대로 처리할 수 있도록 프로그래밍 하였습니다. 해당 방법을 구현하기 위해 ptable구조체에 last_level이라는 변수를 두어 레벨을 이동할 때 마다 이동한 레벨을 저장하도록 하였습니다. (초기 값은 0) 이 후 프로세스를 실행할 때 마다 해당 변수와 현재 실행하려는 프로세스의 레벨을 비교하여 똑같다면 같은레벨이니 그대로 실행 하지만 다른 레벨이라면 실행하는 레벨이 변했다는 뜻이므로 해당 레벨의 프로세스 순서를 원래대로 초기화 하기 위해 해당 레벨의 프로세스들의 birth_tick을 0으로 초기화 했습니다. 이렇게 된다면 프로세스 실행 순서가 pid기준으로 실행되기 때문에 원래의 순서인 들어온 순서대로 실행이 되게 됩니다. 

# Implement

> 본인이 새롭게 구현하거나 수정한 부분에 대해서 무엇이 기존과 어떻게 다른지, 해당 코드가 무엇을 목적으로 하는지에 대한 설명을 구체적으로 서술합니다.
> 

## proc.h

```c
struct proc {
	... 
	int level; // current process queue level
  int priority; // current process priority
  int birth_tick; // 생성된 글로벌 틱 저장
  int remain_quantum; // 남은 퀀컴 수
	int is_lock; // if lock this process, is_lock is 1
}

void update_global_ticks(void);
void schedulerLock(int password)
void schedulerUnlock(int password)
```

- 스케쥴링에 필요한 추가적인 정보를 담기 위해 프로세스 구조체에 변수를 추가해주었습니다.

level : 프로세스의 레벨 정보

priority : 프로세스의 우선순위 정보

birth_tick : 프로세스 실행 순서를 정하기 위한 tick

remain_quantum : 남은 퀀텀의 수

is_lock : lock모드를 요청한 프로세스인지 확인하기 위해

- trap.c에서 proc.c의 함수를 사용하기 위해 헤더파일에 선언해주었습니다.

## proc.c

```c
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  int last_level;
} ptable;
```

ptable 구조체레 이전에 실행한 프로세스의 레벨을 저장할 수 있는 변수를 추가했습니다. 

```c
int time_quantum[3] = {4,6,8}; // each level's time quantum
int global_ticks = 0; // global tick
int lock_mode = 0; // 락 모드가 1 이면 락 중임
```

- 각 레벨의 타임 퀀텀을 배열로 선언해 두었습니다.
- 전역변수로 변경되는 global_ticks를 선언해 두었습니다. 이 변수는 아래의 update 함수를 통해 값이 올라가며, 부스팅이나 lock함수를 통해서도 변경될 수 있습니다.
- 현재 락 모드인지 아닌지 판별할 수 있는 변수를 선언했습니다.

```c
static struct proc* allocproc(void) {
	...
	found:
	...
	p->level = 0; // 초기에 등록되는 프로세스의 레벨을 0으로 지정해준다. 
	p->priority = 3; // 우선순위를 초기값인 3으로 지정해준다. 
	p->birth_tick = global_ticks; // 생성된 global tick을 기록해준다. 
	p->remain_quantum = time_quantum[0]; // 퀀텀을 레벤0에 맞는 값으로 지정해준다.
	p->is_lock = 0; // 초기값 0
	...
}
```

- 처음에 프로세스를 할당해주는 과정에서 추가한 변수들에 대한 초기 값들을 설정해준다.
- 처음에 들어가는 레벨은 0이다.
- 초기 우선순위는 3이다.
- 할당시의 global_ticks를 birth_tick으로 설정해준다.
- is_lock의 초기값은 0 입니다.
- 남은 퀀텀 시간을 레벨 0의 시간으로 설정해준다.

```c
void priority_boosting(){
  // global_ticks 가 100이 되었을 때 초기화 시켜주는 코드이다.
  acquire(&ptable.lock);
  for(struct proc* i = ptable.proc; i < &ptable.proc[NPROC]; i++) {
    // 테이블을 순회하면서 레벨, 우선순위 , 남은 퀀텀을 초기화 시켜준다.
    if(i->state == RUNNABLE) {
      i->level = 0;
      i->priority = 3;
      i->remain_quantum = time_quantum[0];
      // 부스팅 이후 l0->l1->l2 순으로 실행되기 위해 태어난 시각을 조정한다. 
      if(i->level == 0) {
        i->birth_tick = -3;
      } else if (i->level == 1) {
        i->birth_tick = -2;
      } else if (i->level == 2) {
        i->birth_tick = -1;
      }
    }
  }
  release(&ptable.lock);

}
```

- 글로벌 틱이 100이고 lock 모드가 0일때 실행되는 함수이다.
- 테이블을 처음부터 끝까지 순회하면서 RUNNABLE상태인 프로세스에 대해 스케쥴링에 사용되는 프로세스들의 정보를 초기화 시켜준다. 이때 level0 → level1 → level3 이였던 순서대로 실행되야 하기 때문에 birth_tick 값을 각각 다르게 지정해 준다.
- level0 이였던 것은 -3, level1 이였던 것은 -2, level2였던 것은 -3으로 지정해 주어 처음에 실행될 때 모든 프로세스가 level0에 해당함에도 불구하고 이전 레벨 순서대로 실행되게 된다.

```c
void fail_to_lock() {
  // lock 호출을 실패 했을 때 출력해야할..
  // 현재 실행중인 프로세스를 불러와 정보를 출력하고 죽인다.
  struct proc* p = myproc();
  if(p!=0) {
    cprintf("pid: %d, time quantum: %d, level: %d\n", p->pid, p->remain_quantum, p->level);
    p->is_lock = 0;
    kill(p->pid);
  }
}
```

- lock, unlock 함수 실행 시에 학번을 암호로 받아야 하는데 입력이 없거나 틀릴때 해당 함수를 호출한다.
- 실행중인 프로세스의 정보를 출력하고 종료시킨다.

```c
// trap 에서 하나의 ticks가 증가할 때 마다 global tick을 증가 시킨다. 
// 트랩에 해당 함수를 배치해둠
void update_global_ticks() {
  global_ticks++;
  
  if(global_ticks == 100 && lock_mode == 0) {
    // reset ticks
    // 글로벌 틱이 100이 될 때마다 우선순위 부스팅이 일어난다. 
    priority_boosting();
    global_ticks = 0;
  }
}
```

- 전역변수인 global_ticks를 증가하게 하는 함수이다.
- 함수를 호출하는 부분이 trap.c에 존재하여 1tick 이 증가할때마다 글로벌 틱도 증가되게 한다.
- 글로벌 틱이 100에 도달하고 락 모드가 0일 때 우선순위 부스팅을 호출한다.

```c
void reset_tick(int level) {
  for(struct proc* i = ptable.proc; i < &ptable.proc[NPROC]; i++) {
    if(i->state == RUNNABLE && i->level == level) {
      // 변수로 받은 레벨의 프로세스들을 찾고 해당 프로세스의 birth_tick을 0으로 초기화 시킨다.
      i->birth_tick = 0;
    }
  }
}
```

```c
// sys call
void schedulerLock(int password) {
  // lock mode를 1로 바꿔준다.
  // 락 이후에 100 틱 동안 실행 가능이기 때문에 글로벌 틱을 0으로 초기화 싵킨다. 
  if (password == 2019038513) {
    // 암호 확인하기
    global_ticks = 0;
    lock_mode = 1;

    acquire(&ptable.lock);
    struct proc* p = myproc();
    p->is_lock = 1;
    release(&ptable.lock);

  } else {
    // 비밀번호가 틀리면 현재 프로세스를 종료한다.
    fail_to_lock();
  }
}

int sys_schedulerLock(void) {
  // do lock
  int password;
  // 락 모드가 실행중인데 또 락을 실행 할 경우
  if(lock_mode != 0) {
    // 아무 일도 일어나지 않음
    return -1;
  }

  if(argint(0,&password)<0) {
    // 인자가 들어오지 않았을 경우 대비
    fail_to_lock();
    return -1;
  }

  schedulerLock(password);
  return 0;
}

void schedulerUnlock(int password) {
  // lock mode를 0으로 바꿔준다.
  if(password == 2019038513) {
    lock_mode = 0;
  } else {
    // 비밀번호가 틀렸다면 fail_to_lock을 실행한다.
    fail_to_lock();
  }
}

int sys_schedulerUnlock(void) {
  // do lock
  int password;

  if(argint(0,&password)<0) {
    // 인자가 들어오지 않은 경우
    fail_to_lock();
    return -1;
  }

  schedulerUnlock(password);
  return 0;
}
```

- 각각 lock, unlock을 시행하고 호출하는 함수와 warpper 함수이다.
- 함수가 호출 되었을 때 argint를 통해 입력값으로 넘어온 int형 암호를 받는다. 만약에 암호가 틀리거나 아무것도 입력되지 않았으면 fail_to_lock 함수를 호출한다.
- 락 상황인데 락을 호출, 언락상황 인데 언락을 호출할 경우 역시 dail_to_lock 함수를 호출한다.
- 성공을 한다면 lock 혹은 unlock 함수를 호출한다.

```c
int getLevel(void) {
  acquire(&ptable.lock);
  // 현재 실행중인 프로세스를 가져와 레벨을 반환한다. 
  struct proc* p = myproc();
  int result = p->level;
  // cprintf("state %d\n", p->state);
  release(&ptable.lock);
  return result;
}

int sys_getLevel(void) {
  // do getLevel
  int level = getLevel();
  // cprintf("level : %d\n", level);
  return level;
}
```

- getLevel 함수가 호출되었을 때 현재 진행중인 프로세스의 정보를 받아와 현재 속해있는 레벨을 반환한다.

```c
void setPriority(int pid, int priority) {
  acquire(&ptable.lock);
  struct proc* p;
  //pid,우선순위를 받고 테이블을 순회하면서 해당 pid를 가진 프로세스의 우선순위를 변경해준다. 
  for(p = ptable.proc ; p<&ptable.proc[NPROC] ; p++) {
    if(p->pid == pid) {
      p->priority = priority;
      break;
    }
  }
  release(&ptable.lock);
}

int sys_setPriority(void) {
  int pid;
  int priority;
  // cprintf("sys_set_priority");

// 인자로 받은 값들
  if(argint(0,&pid)<0 || argint(1,&priority)<0)
    return -1;

  setPriority(pid, priority);
  return 0;
}
```

- 사용자로부터 pid와 변경 할 우선순위 값을 받아온다. 받아온 값이 없다면 그냥 종료한다. ptable을 순회하면 pid가 같은 프로세스를 찾고 그 프로세스의 우선순위를 변경시켜준다.

```c
for(struct proc* i = ptable.proc; i < &ptable.proc[NPROC]; i++) {
      if(i->is_lock == 1 && i->state == RUNNABLE) {
        // 제일 먼저 lock이 걸려있는 runnable 상태의 프로세스를 찾는다. 
        p = i;
        goto found_lock;
      }
...
}
```

- 제일 먼저 ptable을 순회하면서 RUNNABLE이면서 is_lock = 1인 프로세스를 찾는다.
- 찾았다면 found_lock으로 이동한다.

```c
found_lock:
    // 락 걸린걸 찾았다면 여기로 이동하고 실행한다. 
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    swtch(&(c->scheduler), p->context);
    switchkvm();
    c->proc = 0;

    if(global_ticks == 100) {
        // 글로벌 틱이 100이 되었을 때
        // 현재 실행중인 프로세스의 레벨, 우선순위, 태어난 시각, 남은 퀀텀 정보를 수정해준다.

        p->level = 0;
        p->priority = 3;
        p->birth_tick = -4; // 가장 빨리 실행되야 하므로 -4 로 사기를 친다.
        p->remain_quantum = time_quantum[0];
        p->is_lock = 0;

        for(struct proc* i = ptable.proc; i < &ptable.proc[NPROC]; i++) {
          // 이후 priority boosting이 일어나느데 방금 바꾼 프로세스의 정보는 유지시켜야 함
          if(i->state == RUNNABLE && i->pid != p->pid) {
            i->level = 0;
            i->priority = 3;
            i->remain_quantum = time_quantum[0];
            // 부스팅 이후 l0->l1->l2 순으로 실행되기 위해 태어난 시각을 조정한다. 
            if(i->level == 0) {
              i->birth_tick = -3;
            } else if (i->level == 1) {
              i->birth_tick = -2;
            } else if (i->level == 2) {
              i->birth_tick = -1;
            }
          }
        }
        // 글로벌 틱 0으로 초기화 하고 락 풀고 끄읏
        global_ticks = 0;
        lock_mode = 0;
        release(&ptable.lock);
        continue;

      } else if(lock_mode == 0){
        // 중간에 락 모드가 0으로 바뀌었을 때에 대비한다.
        // 프로세스에 대해 맨 앞으로 오게 하기 위해 값들을 설정한다. 
        p->remain_quantum = time_quantum[0];
        p->level = 0;
        p->priority = 3;
        p->birth_tick = -4;
        p->is_lock = 0;
        release(&ptable.lock);
        continue;
      }
      release(&ptable.lock);
      continue;
```

- 일단 찾을 프로세스에 대하여 실행을 한다. 한 틱씩 실행하면서 락을 해제할 조건에 다다르면 락을 해제한다.
- 글로벌 틱이 100에 도달하였을 때

일단 기본적인 우선순위 부스팅 함수는 lock_mode == 0일때에만 실행되도록 해두었기 때문에 자체적인 우선순위 부스팅이 필요하다. 이때 중요한 것은 기존에 실행되고 있던 프로세스를 level0의 맨 앞에 위치해야 한다. 이를 위해 birth_tick을 제일 낮은 -4로 바꿔준다. 이후 우선순위 부스팅을 하는데 자기 자신의 pid는 부스팅에서 제외하여 변경한 값을 유지한다. 이후 글로벌 틱을 0으로 초기화 하고 락 모드를 0으로 바꾸고 끝낸다. 

- lock_mode가 0이 되었을 때

이 역시 현재 프로세스를 l0의 맨 앞에 오도록 값들을 수정한다. 

```c
// 어떤걸 제일 먼저 실행해야 할지 결정해야 하기 때문에 비교 값들을 일단 선언해둔다. 각각의 비교값들은 각 수치가 가질 수 있는 최대 혹은 최소 값이다. 
    int compare_tick = 101; // 제일 작은걸 찾아야 하기 때문에 최대값과 비교함
    int compare_pid = 37268; // 제일 작은걸 찾아야 하기 때문에 최대값과 비교함
    int compare_priority = 4; // 제일 작은걸 찾아야 하기 때문에 최대값과 비교함
    int found = 0; // 찾았나 못찾았나 결과 값을 표시함

    int level0_count = 0; // 각각의 레벨에 프로세스가 있나 없나 검사함
    int level1_count = 0;
    int level2_count = 0;

    for(struct proc* i = ptable.proc; i < &ptable.proc[NPROC]; i++) {
...
      // 테이블을 순회하면서 실행 가능한 프로세스의 레벨의 조사한다. 
      if(i->state == RUNNABLE) {
        if(i->level == 0) {
          level0_count ++;
        } else if(i->level == 1) {
          level1_count ++;
        } else if(i->level == 2) {
          level2_count ++;
        }
      }
    }

    if(level0_count != 0) {
        // run level 0
        // 제일 빨리 생성된 것을 찾는다.
        for(struct proc* i = ptable.proc; i < &ptable.proc[NPROC]; i++){
          // 순회를 하면서 태어난 시각을 기준으로 제일 작은 것을 찾는다, 
            if(i->state == RUNNABLE && i->level==0 && i->birth_tick < compare_tick) {
                p = i;
                compare_tick = i->birth_tick;
                compare_pid = i->pid;
                found = 1;
            } else if(i->state == RUNNABLE && i->level==0 && i->birth_tick == compare_tick) {
              // 제일 작은게 여러개 일 땨에는 pid를 통해 먼저 태어난 것을 선택한다 .
                if(i->pid < compare_pid) {
                    p=i;
                    compare_pid = i->pid;
                }
            }
        }
        // 레벨0 처리하는 부분으로~
        goto level0;
    }

...
```

- 일반 모드일 때 실행되는 코드이다.
- level에 따른 구조의 큰 차이는 없다. 단지 level2에서만 remain_quantum이 0이 되었을 때 level을 낮춰주는 것이 아닌 우선순위를 변경해준다. (우선순위가 0일땐 다시 0으로)
- 프로세스의 변수 값을 비교하면서 실행할 프로세스를 선정해야 하기 때문에 가장 작은 값을 찾아야 하는 level, priority, birth_tick의 나올 수 있는 최대값을 설정해둔다.
- 가장 먼저 고려할 부분은 level이다. 테이블을 순회하면서 각 레벨에 프로세스가 존재하는지 확인한다. level0에 프로세스가 있다면 level0, level0에 프로세스가 없다면 level1, level1에 프로세스가 없다면 level2에 있는 프로세스를 실행해야 한다.
- 각 레벨에 해당하는 조건문 안으로 들어오면 이후에는 birth_tick을 기준으로 제일 작은 값을 확인한다. birth_tick은 1tick씩 실행될 때마다 global_ticks로 업데이트 되므로 1tick씩 순차적으로 실행될 수 있다.
- priority boosting이 실행된 경우 많은 프로세스의 birth_tick이 같아지므로 pid순으로 선택하여 먼저 생성된 프로세스를 실행하도록 하게 한다.
- 모든 조건은 만족시킨 프로세스를 찾았다면 found 변수를 1로 바꿔주고 각 레벨의 프로세스 실행하는 코드로 넘어간다.

```c
...

level0:

    if(found == 0) {
      // 찾지 못했다면 나가리
        release(&ptable.lock);
        continue;
    }

		if(ptable.last_level != 0) {
      // 현재 레벨은 0인데 이전의 레벨을 저장한 변수 last_level이 0이 아니라면 
      // 레벨이 바뀌었다는 것으로 해당 레벨의 순서를 초기화 시켜주어야 한다. 
      // reset Tick
      reset_tick(0);
      ptable.last_level = 0;
    }

    // 찾은 프로세스를 실행한다. 
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    swtch(&(c->scheduler), p->context);
    switchkvm();
    c->proc = 0;

    p->remain_quantum--; // 실행한 프로세스의 남은 퀀텀을 1 줄인다.
    if(p->state == RUNNABLE && p->remain_quantum == 0) {
      // 남은 퀀텀이 0인데 아직도 runnable이면 다음 레벨로 넘긴다. 
      // 다음 레벨의 맨 뒤로 가야하므로 태어난 시각도 조정해준다. 
      // 타임 퀀텀도 레벨에 맞는 값으로 조정한다. 
        p->level = 1;
        p->remain_quantum = time_quantum[1];
        p->birth_tick = global_ticks;
    } else if(p->state == RUNNABLE && p->remain_quantum != 0) {
        // 퀀텀이 남았다면 해당 레벨의 맨 뒤로 보내야 하기 때문에 tick을 수정한다. 
        p->birth_tick = global_ticks;
    } 
    release(&ptable.lock);
    continue;
```

- 해당하는 프로세스를 찾지 못했다면 다시 위로 올라간다.
- 찾은 프로세스를 실행 한 뒤 프로세스의 변수값들을 바꿔준다.

→ remain_quantum을 1감소 시킨다.

→ 남은 퀀텀이 0인데 RUNNABLE이라는 것은 해당 레벨에서 일을 다 끝내지 못했다는 것 이므로 다음 레벨로 넘기고 remain_quantum을 해당 레벨의 퀀텀으로 바꿔준다. 또한 해당 큐의 맨 뒤로 가야하기 때문에 birth_tick을 업데이트 해준다.

→ 퀀텀이 남았고 RUNNABLE이라면 해당 레벨의 맨 뒤로 보내야 하기 때문에 birth_tick을 업데이트 해준다.

## defs.h

```cpp
void            schedulerLock(int password); 
void            schedulerUnlock(int password);
int             getLevel(void);
void            setPriority(int pid, int priority);
```

## syscall.h

```cpp
#define SYS_schedulerLock 129
#define SYS_schedulerUnlock 130
#define SYS_yield 131
#define SYS_getLevel 132
#define SYS_setPriority 131
```

## syscall.c

```c
extern int sys_schedulerLock(void);
extern int sys_schedulerUnlock(void);
extern int sys_yield(void);
extern int sys_getLevel(void);
extern int sys_setPriority(void);
```

```c
static int (*syscalls[])(void) = {
...
[SYS_schedulerLock] sys_schedulerLock,
[SYS_schedulerUnlock] sys_schedulerUnlock,
[SYS_yield] sys_yield,
[SYS_getLevel] sys_getLevel,
[SYS_setPriority] sys_setPriority,
}
```

## trap.c

```c
void
tvinit(void)
{
  ...
  SETGATE(idt[129], 1, SEG_KCODE<<3, vectors[129], DPL_USER);
  SETGATE(idt[130], 1, SEG_KCODE<<3, vectors[130], DPL_USER);
  ...
}
```

```c
void
trap(struct trapframe *tf)
{
  ...

	if(tf->trapno == 129) {
    // sys_schedulerLock();
    schedulerLock(2019038513);
    exit();
  }
  if(tf->trapno == 130) {
    schedulerUnlock(2019038513);
    exit();
  }

  switch(tf->trapno){   
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      update_global_ticks();
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  ...
}
```

- 129번 → sys_schedulerLock(), 130번 → sys_schedulerUnlock() 을 호출해준다.

이 것이 호출되었다는 것은 interrupt가 호출되었다는 뜻 이므로 인자 값으로 학번을 직접 넘겨준다. 

- ticks를 증가시킬때마다 update_global_ticks();를 호출하여 global_ticks도 같이 증가하게 한다.

## user.h

```cpp
void schedulerLock(int password);
void schedulerUnlock(int password);
void yield(void);
int getLevel(void);
void setPriority(int pid, int priority);
```

## usys.S

```c
SYSCALL(schedulerLock)
SYSCALL(schedulerUnlock)
SYSCALL(yield)
SYSCALL(getLevel)
SYSCALL(setPriority)
```

# Result

> 컴파일 및 실행 과정과, 해당 명세에서 요구한 부분이 정상적으로 동작하는 실행 결과를
첨부하고, 이에 대한 동작 과정에 대해 설명합니다.
> 

![Untitled](ELE3021_project01_12300_2019038513%2089adadb7937d44e28d61192f66389d63/Untitled.png)

# Trouble shooting

> 과제를 수행하면서 마주하였던 문제와 이에 대한 해결 과정을 서술합니다. 혹여 문제를
해결하지 못하였다면 어떤 문제였고 어떻게 해결하려 하였는지에 대해서 서술합니다 .
> 

현제 적용한 방식을 사용하기 이전에 논리적 큐 구조체를 이용해 각 레벨의 프로세스들을 관리하려고 했습니다. 큐 구조를 구현 하기 위해 proc 구조체에 자신의 다음 프로세스를 가리키는 포인터를 추가해주었습니다. 이후 큐 구조체를 다음과 같이 생성하였습니다.

```c
struct proc_queue {
  struct spinlock lock; // 락
  struct proc* head; // 큐의 머리
  struct proc* tail; // 큐의 꼬리
  int size; // 큐의 크기
};

struct proc_queue l0_queue; // 레벨 0
struct proc_queue l1_queue; // 레벨 1
struct proc_queue l2_queue[4]; // 레벨2 의 우선순위 마다 큐 생성
```

이런 식으로 구현 후에 스케쥴러 함수에서는 head에 있는 프로세스를 1tick 실행 후 enqueue → dequeue 를 하여 맨 뒤로 보내는 식으로 구현을 하였습니다. 

그러나 해당 방식을 사용하는 과정에서 문제가 생겼습니다. 스케줄러 함수 사용 시 ptable에 대한 정보를 수정하기 때문에 ptable에 대한 Lock도 얻어야 하고, 큐 구조체에 대한 lock도 각각 얻었어야 했기 때문에 제가 감당할수 없는 수준으로 코드가 상당히 복잡해지고 lock을 어느 타이밍에 얻고 어느 타이밍에 놓아줘야 하는지 잘 알수가 없었고, 테스트 코드를 돌렸을 시에 lock과 관련한 에러가 계속해서 났기에 2일간 피 땀 눈물을 흘리면 고생한 코드를 처음부터 다시 짜게 되었습니다.
