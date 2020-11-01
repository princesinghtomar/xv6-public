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

//  MLFQ variable/functions --------------------------------------------------------


#define MAX_PROC_COUNT (int)9000
// after 10 ticks, process priority is going to increase

// this priority queue holds all runnable process
// it is changed every time scheduler runs
int prioQStart[size_of_the_queue];
int total_size_of_queues[size_of_the_queue];
void delete_func(int qinx_p, int idx);
void push(int qinx_p, struct proc *p);
void dec_priority_func(struct proc *queueinx_p, int retain);
int get_queue_inx(struct proc *currp);
void updateRuntime();
int check_proc(struct proc *p);
int preemption_func(int prio, int checkSamePrio);
int back_index(int qinx_p);
void updating_func();
struct proc *popFront(int qinx_p);
struct proc *getFront(int qinx_p);
int get_ticks_func(struct proc *currp);
void inc_priority_func(struct proc *queueinx_p);
struct proc *storing_queue[size_of_the_queue][MAX_PROC_COUNT];

// end --------------------------------------------------------- 

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
// # ---------------------------------------------------------------- #
  int ticks[5];                // to store the ticks of the process recieved in each queue
  int latestQTime;             // storage of ticks if needed by chance
  int ps_stime;                // ps_time here
  int ps_etime;                // ps_etime here
  int ps_iotime;
  int ps_rtime;                // ps_rtime here
  int etime;                   // End time
  int stime;                   // Start time
  int num_run;                 // number of time process is being executed
  int queue_pos;               // position in  the queue
  int queue_num;               // number of the queue currently present
  int rtime;                   // Runnig time
  int iotime;                  // Input/Output time  
  int priority;                // Priority of the process
  int iotime1;                 // Input/Output time
  int iotime2;                 // Input/Output time
// # ----------------------------------------------------------------- #
};

