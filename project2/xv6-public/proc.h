// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  int limit; // 메모리 제한
  int stack_size;

  int thread_context[64]; // 스레드의 상황을 표시해주는 배열 쓰레드가 있으면 1, 없으면 0이다. 이것을 이용해 메모리 할당 위치를 결정한다. 
  void* thread_ret; // 반환 값
  uint stack_start; // 스택이 시작하는 위치를 기록함
  int thread_top; // 제일 위에 있는 쓰레드의 위를 기록함
  int is_thread; // 쓰레드의 구성원이면 1, 아니면 0
  int num_thread; // 현재 lwp 개수
  int max_thread; // 현재까지 작동 과정 중 가장 위에 있었던 쓰레드를 기록함 -> 메인 스레드의 최대 할당값을 저장
  int tid; // 현재 쓰레드의 tid
  int thread_index; // 쓰레드가 메인 프로세스의 어디에 위치하는지

  struct proc* real_parent; // 쓰레드라면 메인쓰레드의 부모를 가리키게 된다. 

};

void all_kill_thread(struct proc* curproc);

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
