#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  int last_level;
} ptable;

int time_quantum[3] = {4,6,8}; // each level's time quantum
int global_ticks = 0; // global tick
int lock_mode = 0; // 락 모드가 1 이면 락 중임

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  p->level = 0; // 초기에 등록되는 프로세스의 레벨을 0으로 지정해준다. 
  p->priority = 3; // 우선순위를 초기값인 3으로 지정해준다. 
  p->birth_tick = global_ticks; // 생성된 global tick을 기록해준다. 
  p->remain_quantum = time_quantum[0]; // 퀀텀을 레벤0에 맞는 값으로 지정해준다.
  p->is_lock = 0; // 초기값 0 

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

// my code!!!!
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

void reset_tick(int level) {
  for(struct proc* i = ptable.proc; i < &ptable.proc[NPROC]; i++) {
    if(i->state == RUNNABLE && i->level == level) {
      // 변수로 받은 레벨의 프로세스들을 찾고 해당 프로세스의 birth_tick을 0으로 초기화 시킨다.
      i->birth_tick = 0;
    }
  }
}

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

int getLevel(void) {
  acquire(&ptable.lock);
  // 현재 실행중인 프로세스를 가져와 레벨을 반환한다. 
  struct proc* p = myproc();
  int result = p->level;
  release(&ptable.lock);
  return result;
}

int sys_getLevel(void) {
  int level = getLevel();
  return level;
}

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

// 인자로 받은 값들
  if(argint(0,&pid)<0 || argint(1,&priority)<0)
    return -1;

  setPriority(pid, priority);
  return 0;
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

// 어떤걸 제일 먼저 실행해야 할지 결정해야 하기 때문에 비교 값들을 일단 선언해둔다. 각각의 비교값들은 각 수치가 가질 수 있는 최대 혹은 최소 값이다. 
    int compare_tick = 101; // 제일 작은걸 찾아야 하기 때문에 최대값과 비교함
    int compare_pid = 37268; // 제일 작은걸 찾아야 하기 때문에 최대값과 비교함
    int compare_priority = 4; // 제일 작은걸 찾아야 하기 때문에 최대값과 비교함
    int found = 0; // 찾았나 못찾았나 결과 값을 표시함

    int level0_count = 0; // 각각의 레벨에 프로세스가 있나 없나 검사함
    int level1_count = 0;
    int level2_count = 0;

    for(struct proc* i = ptable.proc; i < &ptable.proc[NPROC]; i++) {
      if(i->is_lock == 1 && i->state == RUNNABLE) {
        // 제일 먼저 lock이 걸려있는 runnable 상태의 프로세스를 찾는다. 
        p = i;
        goto found_lock;
      }
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
    } else if(level1_count != 0) {
        // run level 1
        // 레벨 0의 처리 과정과 동일하다. 
        for(struct proc* i = ptable.proc; i < &ptable.proc[NPROC]; i++){
            if(i->state == RUNNABLE && i->level==1 && i->birth_tick < compare_tick) {
                p = i;
                compare_tick = i->birth_tick;
                found = 1;
            } else if(i->state == RUNNABLE && i->level==1 && i->birth_tick == compare_tick) {
                if(i->pid < compare_pid) {
                    p=i;
                    compare_pid = i->pid;
                }
            }
        }
        goto level1;
    } else if(level2_count != 0) {
        // run level 2
        for(struct proc* i = ptable.proc; i < &ptable.proc[NPROC]; i++){
          // 레벨2 의 경우 비교하는 값이 조금 다르다. 
          // 여기는 우선순위가 중요하기 때문에 우선순위를 먼저 비교하고
            if(i->state == RUNNABLE && i->level==2 && i->priority < compare_priority) {
                p = i;
                compare_tick = i->birth_tick;
                compare_pid = i->pid;
                found = 1;
            } else if(i->state == RUNNABLE && i->level==1 && i->priority == compare_priority) {
              // 우선순위가 같다면 태어난 시각이 더 빠른 것을
                if(i->birth_tick < compare_tick) {
                    p=i;
                    compare_pid = i->pid;
                    compare_tick = i->birth_tick;
                } else if(i->birth_tick == compare_tick) {
                  // 태어난 시각조차 같다면 pid를 기준으로
                    if(i->pid < compare_pid) {
                        p=i;
                        compare_pid = i->pid;
                    }
                }
            }
        }
        goto level2;
    } else {
        release(&ptable.lock);
        continue;
    }

    found_lock:
    // 락 걸린걸 찾았다면 여기로 이동하고 실행한다. 
    // cprintf("locked : remain q : %d, pid : %d , lock : %d\n",p->remain_quantum, p->pid, p->is_lock);
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
        ptable.last_level = 0;
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
        ptable.last_level = 0;
        release(&ptable.lock);
        continue;
      }
      release(&ptable.lock);
      continue;

    level0:

    if(found == 0) {
      // 찾지 못했다면 나가리이다.
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

    level1:
    // 레벨1 도 레벨0과 동일한 과정을 거친다. 

    if(found == 0) {
        release(&ptable.lock);
        continue;
    } 

    if(ptable.last_level != 1) {
      // reset Tick
      reset_tick(1);
      ptable.last_level = 1;
    }
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    swtch(&(c->scheduler), p->context);
    switchkvm();
    c->proc = 0;

    p->remain_quantum--;
    if(p->state == RUNNABLE && p->remain_quantum == 0) {
        p->level = 2;
        p->remain_quantum = time_quantum[2];
        p->birth_tick = global_ticks;
    } else if(p->state == RUNNABLE && p->remain_quantum != 0) {
        p->birth_tick = global_ticks;
    }
    release(&ptable.lock);
    continue;

    level2:
    // 레벨 2도 비슷한 과정이지만 일부 다른 부분이 있다. 
    if(found == 0) {
        release(&ptable.lock);
        continue;
    }

    if(ptable.last_level != 2) {
      // reset Tick
      reset_tick(2);
      ptable.last_level = 2;
    }

    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    swtch(&(c->scheduler), p->context);
    switchkvm();
    c->proc = 0;

    p->remain_quantum--;
    if(p->state == RUNNABLE && p->remain_quantum == 0) {
        p->level = 2;
        p->remain_quantum = time_quantum[2];
        p->birth_tick = global_ticks;
        // 우선순위가 0이 아니라면 1감소 시켜 우선순위를 높여준다. 
        if(p->priority != 0) {
            p->priority --;
        }
    } else if(p->state == RUNNABLE && p->remain_quantum != 0) {
        // p->remain_quantum = time_quantum[1];
        p->birth_tick = global_ticks;
    } 
    release(&ptable.lock);
    continue;
    // release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}



