#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "elf.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
int nexttid = 1;
// int thread_count = 0;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

// 내가만든 함수들~!~!~!~!~
int
exec2(char *path, char **argv, int stacksize)
{
    if(stacksize<1 || stacksize>100) {
      // 올바르지 않은 범위
      cprintf("exec: wrong stack size\n");
      return -1;
    }
    // 기본적으로 exec.c의 코드를 그대로 가져왔고 메모리 할당하는 과정에서 입력받은 스택의 사이즈 적용하는 부분만 다르다. 
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();

  //cprintf("%s\n", path);

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail1\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
  // 가?상 메모리를 불러오시는 듯
    goto bad;
 //cprintf("exec2\n");
  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + (stacksize+1)*PGSIZE)) == 0)
  // 내가 원하는 크기 + 1 만큼 일단 할당 받아보자~
  // 기존의 sz에서 새롭게 할당 받은 크기만큼 받고 새로운 sz 반환
    goto bad;
  clearpteu(pgdir, (char*)(sz - (stacksize+1)*PGSIZE)); // 원하는 스택 개수 + 가드용 페이지 할당
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

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

  switchuvm(curproc);
  freevm(oldpgdir);
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

int sys_exec2(void){
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;
  int stacksize;
  // 밑의 부분도 기존의 sys_exec에 stacksize를 받는 부분만 추가해주었다.

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0 || argint(2,&stacksize)<0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec2(path, argv, stacksize);
}



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

  if(p->sz >limit || limit<0) {
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

int sys_setmemorylimit(void){
  int pid,limit;
  if(argint(0, &pid) < 0 || argint(1,&limit)<0)
  cprintf("set memory : argint error\n");
    return -1;
  return setmemorylimit(pid,limit);
}


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

int sys_manager_list(void) {
  manager_list();
  return 0;
}


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
  // 초기화 시에 쓰레드가 있는지 없는지를 나타내는 배열을 0으로 초기화 한다. 
  for(int i=0 ; i<64 ; i++) {
    p->thread_context[i] = 0;
  }
  p->limit = 0;
  p->stack_size = 1;
  for(int i=0 ; i<64 ; i++) {
    p->thread_context[i] = 0;
  }
  p->stack_start = p->sz;
  p->is_thread = 0;
  p->num_thread = 0;
  p->max_thread = 64;
  p->tid = 0;

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
  struct proc *p;

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

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  //release(&ptable.lock);
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

  np->sz = curproc->sz;
  
  np->parent = curproc;
  np->real_parent = curproc; 
  
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
  np->is_thread = 0;
  np->num_thread = 0;

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


  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){ // && p->is_thread == 0
      p->parent = initproc;
      if(p->state == ZOMBIE) {
        cprintf("found zonbie %s\n", p->name);
        wakeup1(initproc);
      }
        
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
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

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
    if(p->pid == pid ){
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
  // cprintf("%d\n", p->sz);
  release(&ptable.lock);
  // cprintf("complete celate\n");
  return 0;
}

int sys_thread_create(void) {
  int thread;
  int start_routine;
  int arg;

  if((argint(0, &thread) < 0) || (argint(1, &start_routine) < 0) || (argint(2, &arg) < 0)) {
    return -1;
  }
	
  int result;
  result = thread_create((thread_t*)thread, (void *)start_routine, (void *)arg);
  return result;
}

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

int sys_thread_join(void){
  int thread;
  int retval;

  if((argint(0, &thread) < 0) || (argint(1, &retval) < 0)) {
    return -1;
  }

  thread_join((thread_t)(unsigned int)thread, (void**)retval);

  return 0;

}
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

int sys_thread_exit(void) {
  int retval;
  if(argint(0,&retval) < 0) {
    return -1;
  }
  thread_exit((void*)retval);
  return 1;
}

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




