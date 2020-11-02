#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

//TODO Remember!! 1,2,4,8,16, ticks to move to different queues

int inc_queue_array[5] = {86, 85, 84, 83, 82};
static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int cpuid()
{
  return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu *
mycpu(void)
{
  int apicid, i;

  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i)
  {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc *
myproc(void)
{
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

int min_function(int x, int y)
{
  return x > y ? y : x;
}

void initProcMyStyle(struct proc *p)
{
  int temp_var = (p->pid == 0);
  if (!p || temp_var)
    return;

    // else continue;

#ifdef MLFQ
  int cur_ticks = ticks; //problem can be here if it happens
  push(min_index_queue, p);
  p->latestQTime = cur_ticks;

#endif
  p->etime = -1;
  p->ps_etime = -1;
  p->rtime = 0;
  p->ps_rtime = 0;
  p->ps_iotime = 0;
  p->stime = ticks;
  p->ps_stime = ticks;
}

//TODO check for errors

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->etime = 0;
  p->ps_stime = ticks;
  p->ps_etime = 0;
  p->pid = nextpid++;
  p->ps_iotime = 0;
  p->ps_rtime = 0;
  p->stime = ticks;
  p->iotime = 0;
  p->rtime = 0;

  p->num_run = 0;
  for (int i = 0; i < 5; ++i)
    p->ticks[i] = -1;

#ifdef MLFQ
  p->priority = 60;
  for (int i = 0; i < 5; ++i)
    p->ticks[i] = 0;
#endif
#ifdef RR
  p->priority = -1;
#endif
#ifdef FCFS
  p->priority = -1;
#endif
#ifdef PBS
  p->priority = 60;
#endif

  release(&ptable.lock);

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0)
  {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  initProcMyStyle(p);

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
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
  p->tf->eip = 0; // beginning of initcode.S

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
int growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

/////

int preemption_func(int prio, int checkSamePrio)
{
  struct proc *p;
  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (!(p->state == RUNNABLE))
    {
      continue;
    }
#ifdef MLFQ
    int q = get_queue_inx(p);
    int check_var2 = (q == prio && checkSamePrio);
    int check_var1 = (q < prio);
    if (q < 0)
    {
      continue;
      // that means its empty so continue
    }
    if (check_var1 || check_var2)
    {
      release(&ptable.lock);
      return p->pid;
    }
#endif
#ifndef MLFQ
    int check_var4 = (checkSamePrio && p->priority == prio);
    int check_var3 = (prio > p->priority);
    if (check_var3 || check_var4)
    {
      release(&ptable.lock);
      return 1;
    }
#endif
  }
  release(&ptable.lock);
  return 0;
}
/////

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy process state from proc.
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
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

  for (i = 0; i < NOFILE; ++i)
    if (curproc->ofile[i])
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
void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++)
  {
    if (curproc->ofile[fd])
    {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  //TODO check this its correct or not
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  curproc->etime = ticks;
  curproc->ps_etime = ticks;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
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
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock); //DOC: wait-sleep
  }
}

int waitx(int *wtime, int *rtime)
{
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Found one.
        int sub_time;
        *rtime = p->rtime;
        *wtime = p->etime;
        sub_time = p->stime + p->rtime;
        *wtime = *wtime - sub_time;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        pid = p->pid;
        p->name[0] = 0;
        p->state = UNUSED;
        p->killed = 0;
        p->parent = 0;
        p->pid = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (havekids == 0 || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock); //DOC: wait-sleep
  }
}

int check_proc(struct proc *p)
{
  if (!p)
  {
    return 1;
  }
  if (p->killed)
  {
    return 1;
  }
  int flag1 = p->pid == 0;
  int flag2 = p->state == RUNNABLE;
  int ret_val = flag1 || !flag2;
  return ret_val;
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

#ifdef RR
void scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;
  struct proc *p;

  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (!(p->state == RUNNABLE))
      {
        continue;
      }
      else
      {
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        c->proc = p;
        switchuvm(p);
        if (!(p->state == RUNNING))
        {
          p->state = RUNNING;
        }
        p->num_run = p->rtime; // i like this
        // for checkers :-
        // or simply do p->num_run +=1 this will also work

        swtch(&(c->scheduler), p->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
    }
    release(&ptable.lock);
  }
}
#endif

#ifdef FCFS
void scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;
  struct proc *p;
  for (;;)
  {
    sti();
    struct proc *temp_proc = 0;
    struct proc *selected_one = 0;
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (!(p->state == RUNNABLE))
        continue;
      else
      {
        if (!temp_proc)
        {
          temp_proc = p;
        }
        else
        {
          if (temp_proc->stime > p->stime) // check and pick the one having lowest
            temp_proc = p;                 // lowest starting time
        }
      }
      selected_one = temp_proc;
    }
    if (selected_one)
    {
      c->proc = selected_one;
      switchuvm(selected_one);
      if (!(selected_one->state == RUNNING))
      {
        selected_one->state = RUNNING;
      }
      selected_one->num_run += 1;
      swtch(&(c->scheduler), selected_one->context);
      switchkvm();
      c->proc = 0;
    }
    release(&ptable.lock);
  }
}
#endif

#ifdef PBS
void scheduler(void)
{
  struct cpu *c = mycpu();
  struct proc *p;
  c->proc = 0;

  for (;;)
  {
    // for all the default comments refer RR function
    sti();
    struct proc *highP = 0;
    struct proc *tempP = 0;
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (!(p->state == RUNNABLE))
        continue;
      highP = p;
      //choose one with highest priority
      for (struct proc *p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++)
      {
        if (!(p1->state == RUNNABLE))
          continue;
        if (!highP)
        {
          highP = p;
        }
        else
        {
          if (highP->priority > p1->priority) //larger value, lower priority
            highP = p1;
        }
      }
      if (!pass)
      {
        panic("Problem in pass");
      }
      tempP = highP;
      int storeP = tempP->priority;
      //p = highP;
      c->proc = tempP;
      switchuvm(tempP);
      if(!(tempP->state==RUNNING)){
        tempP->state = RUNNING;
      }
      tempP->num_run += 1;

      swtch(&(c->scheduler), tempP->context);
      switchkvm();
      c->proc = 0;
      highP = p;
      for (struct proc *p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++)
      {
        if (!(p1->state == RUNNABLE))
          continue;
        if (!highP)
        {
          highP = p;
        }
        else
        {
          if (highP->priority > p1->priority) //larger value, lower priority
            highP = p1;
        }
      }
      if (highP->priority < storeP)
      {
        break;
      }
    }
    release(&ptable.lock);
  }
}
#endif

#ifdef MLFQ

void scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;
  struct proc *p;

  for (;;)
  {
    sti();
    struct proc *selected_proc = 0;
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (!(p->state == RUNNABLE))
      {
        continue;
      }
      if (p->state == RUNNABLE && p > 0)
      {
        if (!(get_queue_inx(p) == -1)) // process allocated hence push it back to the queue
        {
          if (!total_size_of_queues[get_queue_inx(p)])
          {
            push(get_queue_inx(p), p);
          }
        }
      }
    }
    for (int i = 0; i < 5; ++i) // check for all the queue for processes
    {
      while (total_size_of_queues[i] > 0)
      {
        struct proc *p = getFront(i);

        if (!check_proc(p)) // check whether process is alive or not
        {
          selected_proc = p;
          break;
        }
        else // if dead pop it from queue
        {
          popFront(i);
          p = 0;
        }
      }

      if (!selected_proc)
      { // if not alotted then continue
        continue;
      }
      else
      { // break if got a process
        break;
      }
    }
    if (selected_proc)
    {
      if (!pass)
      {
        panic("Problem in pass");
      }
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = selected_proc;
      switchuvm(selected_proc);

      // removing running process from queue
      popFront(get_queue_inx(selected_proc));
      selected_proc->latestQTime = ticks;
      selected_proc->num_run += 1;
      if (!(selected_proc->state == RUNNING))
      {
        selected_proc->state = RUNNING;
      }

      swtch(&(c->scheduler), selected_proc->context);

      // technically it should be pushing at the back of the same
      // queue if it had not yield
      // if process went to sleep or was not able to complete its full
      // time slice, push it to end of same queue
      int val_variable1 = ticks - selected_proc->latestQTime;
      int procTcks = val_variable1;
      int queueinx_p = get_queue_inx(selected_proc);
      int var_flag1 = selected_proc->state == RUNNABLE && procTcks > 0;
      if ((selected_proc->state == SLEEPING) || ((procTcks < (1 << queueinx_p)) && var_flag1))
      {
        dec_priority_func(selected_proc, 1);
      }
      else if ((procTcks == (1 << queueinx_p)) && selected_proc->state == RUNNABLE)
      {
        dec_priority_func(selected_proc, 0);
      }

      switchkvm();

      // Process is done running for now. It should have changed its
      // p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
  }
}
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (mycpu()->ncli != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  acquire(&ptable.lock); //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first)
  {
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
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if (p == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock)
  {                        //DOC: sleeplock0
    acquire(&ptable.lock); //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  p->iotime1 = ticks;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock)
  { //DOC: sleeplock2
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

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == SLEEPING && p->chan == chan)
    {
#ifdef MLFQ
      push(get_queue_inx(p), p);
#endif
      p->iotime2 = ticks;
      p->state = RUNNABLE;
      p->iotime += p->iotime2 - p->iotime1;
    }
  }
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
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
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; ++i)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

#ifdef MLFQ

int increment_p_func(struct proc *p)
{
  if (!p)
  {
    return 0;
  }
  int tcks = get_ticks_func(p);
  int qinx_p = get_queue_inx(p);
  tcks += 1;

  if (p->state == RUNNABLE && p)
  {
    if (inc_queue_array[qinx_p] <= tcks)
    {
      inc_priority_func(p);
    }
    else
    {
      return 0;
    }
  }
  return 0;
}


void updating_func()
{
  struct proc *p;
  if (cpuid() != 0)
    panic("Must be called from cpu0 only");

  ticks++;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    int flag_temp = (p || p->pid);
    if (!flag_temp)
    {
      continue;
    }
    if (p->killed == 1)
    {
      continue;
    }
    if (p && (p->state == RUNNABLE || p->state == RUNNING))
    {
      increment_p_func(p);
    }
  }
  release(&ptable.lock);
}

struct proc *getFront(int qinx_p)
{
  int flag_val = total_size_of_queues[qinx_p];
  if (!flag_val)
  {
    panic("A new problem found in size of queue");
  }
  struct proc *p = storing_queue[qinx_p][prioQStart[qinx_p]];
  if (!p)
  {
    panic("struct proc *p is empty");
  }
  return p;
}

int popFront2(int qinx_p)
{
  prioQStart[qinx_p] += 1;
  if (prioQStart[qinx_p] != MAX_PROC_COUNT)
  {
    //now since a process is gone, hence make its default queue to '0'
  }
  else
  {
    if (!(prioQStart[qinx_p] == 0))
    {
      prioQStart[qinx_p] = 0;
    }
  }
  total_size_of_queues[qinx_p] -= 1;
  //decrese size of the queue
  return 1;
}

struct proc *popFront(int qinx_p)
{
  int flag_val = total_size_of_queues[qinx_p];
  if (!flag_val)
  {
    panic("Empty stack,hence not possible to pop");
  }

  // DO NOT CHANGE allotted q since they help in memory
  struct proc *p = getFront(qinx_p);
  if (!p)
  {
    panic("struct proc *p is empty");
  }
  int value_ret = popFront2(qinx_p);
  if (value_ret)
  {
    return p;
  }
  else
  {
    panic("Problem in popFront2 funct");
  }
}

// index used to insert new elements into the queue
int back_index(int qinx_p)
{
  int val2_p = total_size_of_queues[qinx_p];
  int val1_p = prioQStart[qinx_p];
  int fla_val = (val1_p + val2_p);
  if (fla_val < MAX_PROC_COUNT)
  {
    return fla_val;
  }
  else
  {
    return fla_val % MAX_PROC_COUNT;
  }
}

int push1(struct proc *p, int qinx_p)
{
  if (p)
    ;
  else
  {
    panic("NULL proc !!");
    return 0;
  }
  int bi = back_index(qinx_p);
  p->queue_num = qinx_p,
  p->queue_pos = bi,
  storing_queue[qinx_p][p->queue_pos] = p;
  total_size_of_queues[qinx_p] += 1;
  return 1;
}
void push(int qinx_p, struct proc *p)
{
  int i;
  if (p)
  {
    ;
  }
  else
  {
    panic("the proc passed is empty hence failed to push");
  }

  for (i = prioQStart[qinx_p]; i != back_index(qinx_p); i++, i = i % MAX_PROC_COUNT)
  {
    if (storing_queue[qinx_p][i]->pid == p->pid)
    {
      if (storing_queue[qinx_p][i])
      {
        return;
      }
    }
  }
  int val = push1(p, qinx_p);
  if (val == 0)
  {
    panic("Problem in passing argument");
  }
}

int getQPos(struct proc *currp)
{
  return currp->queue_pos;
}

int get_ticks_func(struct proc *currp)
{
  int temp_val = currp->latestQTime;
  int tck = ticks;
  if (temp_val < 0)
  {
    panic("the varible latestQTime can't be -ve");
  }
  int val_ret = tck - temp_val;
  return val_ret;
}

void delete_func(int qinx_p, int idx)
{
  if (storing_queue[qinx_p][idx] == 0)
  {
    panic("Some deleted index found");
  }
  storing_queue[qinx_p][idx] = 0;
  total_size_of_queues[qinx_p] = total_size_of_queues[qinx_p] - 1;
  int bi = back_index(qinx_p), i;
  for (i = idx; !(i == bi); i++, i = i % MAX_PROC_COUNT)
  {
    int val_temp = (i + 1) % MAX_PROC_COUNT;
    storing_queue[qinx_p][i] = storing_queue[qinx_p][val_temp],
    storing_queue[qinx_p][i]->queue_pos = i;
  }
}
int get_queue_inx(struct proc *currp)
{
  int val_ret = currp->queue_num;
  if (val_ret >= 0 || val_ret < 5)
    ;
  else
  {
    panic("Wrong queue id found in get queue func");
  }
  return val_ret;
}


void dec_priority_func(struct proc *currp, int retain)
{
  int queueinx_p = get_queue_inx(currp);
  if (queueinx_p < 0)
    panic("Problem in queueinx in incp func");

  if (currp)
    ; //check for the currp whether its NULL or not
  else
    panic("currp can't be zero in inc_p function");

  currp->latestQTime = ticks;
  int dest = queueinx_p;

  if (queueinx_p == size_of_the_queue - 1 || retain)
  {
    push(queueinx_p, currp);
  }
  else
  {
    currp->ps_stime = ticks;
    currp->ps_rtime = 0;
    queueinx_p += 1;
    currp->ps_etime = ticks;
    if (queueinx_p == 0)
    {
      currp->priority = 15;
    }
    else if (queueinx_p == 1)
    {
      currp->priority = 35;
    }
    else if (queueinx_p == 2)
    {
      currp->priority = 55;
    }
    else if (queueinx_p == 3)
    {
      currp->priority = 75;
    }
    else if (queueinx_p == 4)
    {
      currp->priority = 95;
    }
    push(queueinx_p, currp);
    dest = dest + 1;
  }
}

void inc_priority_func(struct proc *currp)
{
  int queueinx_p = get_queue_inx(currp);
  int qPos = getQPos(currp);
  if (queueinx_p < 0 || qPos < 0)
  {
    cprintf("%d %d\n", queueinx_p, qPos);
    panic("Problem in queueinx in incp func");
  }
  delete_func(queueinx_p, qPos);

  if (currp)
    ;
  else
  {
    panic("currp can't be zero in inc_p function");
  }
  int dest = queueinx_p;
  if (!queueinx_p)
  {
    currp->priority = 15;
    push(queueinx_p, currp);
  }
  else
  {
    dest = dest - 1,
    queueinx_p -= 1;
    currp->ps_stime = ticks;
    currp->ps_rtime = 0;
    currp->ps_iotime = 0;
    currp->ps_etime = ticks;
    if (queueinx_p == 0)
    {
      currp->priority = 15;
    }
    else if (queueinx_p == 1)
    {
      currp->priority = 35;
    }
    else if (queueinx_p == 2)
    {
      currp->priority = 55;
    }
    else if (queueinx_p == 3)
    {
      currp->priority = 75;
    }
    else if (queueinx_p == 4)
    {
      currp->priority = 95;
    }
    push(queueinx_p, currp);
  }
}

#endif

int ps()
{
  struct proc *p;
  //Enabling interrupts on this processor.
  sti();

  //Loop over process table looking for process with pid.
  acquire(&ptable.lock);
  cprintf("pid \t state \t\t priority \t r_time \t w_time \t n_run "
          "\t cur_q \t q0 \t q1 \t q2 \t q3 \t q4 \t name \n");
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    int rtime;
    int wtime;
    wtime = p->etime - p->ps_stime - p->ps_rtime - p->ps_iotime;
    rtime = p->rtime;
    if (p->state != ZOMBIE)
    {
      wtime = ticks - p->ps_stime - p->ps_rtime - p->iotime;
    }
    if (wtime < 0)
    {
      wtime = 0;
    }
    if (p->state == SLEEPING)
    {
      cprintf("%d \t SLEEPING \t %d \t\t %d \t\t %d \t\t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %s \n ",
              p->pid, p->priority, rtime, wtime, p->num_run, p->queue_num, p->ticks[0],
              p->ticks[1], p->ticks[2], p->ticks[3], p->ticks[4], p->name);
    }
    else if (p->state == RUNNING)
    {
      cprintf("%d \t RUNNING \t %d \t\t %d \t\t %d \t\t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %s \n ",
              p->pid, p->priority, rtime, wtime, p->num_run, p->queue_num, p->ticks[0],
              p->ticks[1], p->ticks[2], p->ticks[3], p->ticks[4], p->name);
    }
    else if (p->state == ZOMBIE)
    {
      cprintf("%d \t ZOMBIE \t %d \t\t %d \t\t %d \t\t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %s \n ",
              p->pid, p->priority, rtime, wtime, p->num_run, p->queue_num, p->ticks[0],
              p->ticks[1], p->ticks[2], p->ticks[3], p->ticks[4], p->name);
    }
    else if (p->state == RUNNABLE)
    {
      cprintf("%d \t RUNNABLE \t %d \t\t %d \t\t %d \t\t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %s \n ",
              p->pid, p->priority, rtime, wtime, p->num_run, p->queue_num, p->ticks[0],
              p->ticks[1], p->ticks[2], p->ticks[3], p->ticks[4], p->name);
    }
    else if (p->state == EMBRYO)
    {
      cprintf("%d \t EMBRYO \t \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %s\n ",
              p->pid, p->priority, rtime, wtime, p->num_run, p->queue_num, p->ticks[0],
              p->ticks[1], p->ticks[2], p->ticks[3], p->ticks[4], p->name);
    }
    else if (p->state == UNUSED)
    {
      // do nothing as no need
    }
  }
  release(&ptable.lock);
  return 0;
}

int chpr(int priority, int pid)
{
  struct proc *p;
  int flag = 0;
  int old_priority;
  if (!(priority >= 0) || !(priority <= 100))
  {
    return -2;
  }
  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      old_priority = p->priority;
      p->priority = priority;
      flag++;
      break;
    }
  }
  release(&ptable.lock);
  if (flag == 0)
  {
    return -1;
  }
  else
  {
    return old_priority;
  }
}

void updateRuntime()
{
  struct proc *p;
  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == RUNNING)
    {
      p->rtime += 1;
      p->ps_rtime += 1;
      //cprintf("%d %d %d\n",ticks,p->pid,p->queue_num);
    }
    if (p->state == SLEEPING)
    {
      p->iotime += 1;
      p->ps_iotime += 1;
    }
  }
  release(&ptable.lock);
}
