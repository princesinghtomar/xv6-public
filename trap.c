#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[]; // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void tvinit(void)
{
  int i;

  for (i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE << 3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE << 3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void trap(struct trapframe *tf)
{
  if (tf->trapno == T_SYSCALL)
  {
    if (myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if (myproc()->killed)
      exit();
    return;
  }

  switch (tf->trapno)
  {
  case T_IRQ0 + IRQ_TIMER:
    if (cpuid() == 0)
    {
      acquire(&tickslock);
#ifndef MLFQ
      ticks++;
#endif
#ifdef MLFQ
      updating_func();
#endif
      //updateRuntime();
      wakeup(&ticks);
      release(&tickslock);
      //changes
      //if (myproc())
      {
        updateRuntime();
      }
      //changes
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE + 1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if (myproc() == 0 || (tf->cs & 3) == 0)
    {
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();

#ifdef PBS
  if (myproc())
  {
    int allow = 1;
    if (myproc()->state == RUNNING)
    {
      int flag_val = T_IRQ0 + IRQ_TIMER;
      if (tf->trapno == flag_val)
      {
        if (preemption_func(myproc()->priority, allow))
        {
          yield();
        }
      }
    }
  }

#endif
#ifdef RR
  if (myproc())
  {
    if (myproc()->state == RUNNING)
    {
      int flag_val = T_IRQ0 + IRQ_TIMER;
      if (tf->trapno == flag_val)
      {
        yield();
      }
    }
  }
#endif
#ifdef MLFQ
  struct proc *currp = myproc();
  int flag_val1 = T_IRQ0 + IRQ_TIMER, flag_val2;
  int allow = 0, queue_num, tcks;
  if (currp && tf->trapno == flag_val1)
  {
    if (currp->state == RUNNING)
    {
      queue_num = currp->queue_num,
      currp->ticks[queue_num] += 1;
    }
    int queueinx_p = get_queue_inx(currp);

    if (queueinx_p < allow)
    {
      cprintf("%d %d\n", queueinx_p, currp->pid);
      panic("Problem in queue alloting");
    }

    tcks = get_ticks_func(currp);

    if (currp->state == RUNNING)
    {
      flag_val2 = 1 << queueinx_p;
      // do a round robin, my time slice is over
      if (tcks)
      {
        if (tcks >= flag_val2)
        {
          yield();
        }
      }
      else
      {
        int x = preemption_func(queueinx_p, allow);
        if (x)
        {
          yield();
        }
      }
    }
  }

#endif
//#ifndef FCFS
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();
//#endif
}
