# Assignment 5


### Content

- [Task#1]
- [part(a)]
- [part(b)]
- [Task#2]
- [part(a)]
- [part(b)]
- [part(c)]
- [Bonus]

# Task#1

# Part(a) :
waitx function :

unit of time = # of ticks

Defined variables in the proc stat :
    a) stime {**start_time**}
    b) etime {**end_time**}
    c) rtime {**run_time**}

in proc.c :-

```
    // in waitx function 

        *rtime = p->rtime;              //rtime = run_time = p->rtime
        *wtime = p->etime;              //*wtime  = end-time
        sub_time = p->stime + p->rtime; //time to be subtracted from *wtime
        *wtime = *wtime - sub_time;     //subtract sub_time from wtime
```

```
    //in updateRuntime function called from trap.c
    //for updating r-time if process is running

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
        if (p->state == RUNNING)
            p->rtime++;
    }

```

```
    //all of them are updating in allocproc while creating the processes
    //etime updated in exit function 

```

# part(b) :-

The ps function :

  The **ps function** in proc.c is the implementation of **ps syscall**
  In this function i used if else to check for the state of the processes and print
  the proc stat variables as needed. The wait time in ps function differs from wait
  time of waitx as sleeping time is counted in waitx while sleeping time not counted
  in ps function. 


# Task#2 :-

# Part(a) {FCFS}:-
    Logic :- looped over the queue if the function comes out to be runnable then just
             ran over it, if the process leaves the cpu then next process is executed
             For example a PC with 2 cpu cores can run only two process max at a time
             not more then that.The algorithm used is non-preemtive.

```
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
          if (temp_proc->stime > p->stime)
            temp_proc = p;
        }
      }
      selected_one = temp_proc;
    }
    if (selected_one)       // here running the selected process
    {
      c->proc = selected_one;
      switchuvm(selected_one);
      selected_one->state = RUNNING;
      swtch(&(c->scheduler), selected_one->context);
      switchkvm();
      c->proc = 0;
    }
    release(&ptable.lock);
}

```

# Part(b) {PBS} :-
    Logic :- Looped over the queue of processes to find the process with maximum 
             priority if the process with maximum priority is found then its is
             being executed, if more then one such processes with same priority 
             then then RR is used else the process with max priority is given 
             priviledge, in the previously executed Round Robin(xv6's real schedular).
             if some new process comes with lower priority then the sedular restart
             and again executes in the previous way,but with new max priority.The
             algorithm used is non-preemtive.In this scheduling algo the process with
             least value of proc->priority (i.e. highest actual priority) is given 
             high priviledge.That means if there's a high priority in the queue then 
             it'll be executed first. then others will be executed.One important thing
             the priority are not given to the processes they are treated equally,
             Checker is expected to give priority using setPriority function.

code :-

```
void scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;
  struct proc *p;
  for (;;)
  {
    sti();
    struct proc *highP = 0;
    struct proc *tempP = 0;
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (!(p->state == RUNNABLE))
        continue;
      highP = p;
      for (struct proc *p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++)
      {
        if (p1->state != RUNNABLE)
          continue;
        if (!highP)
        {
          highP = p;
        }
        else
        {
          if (highP->priority > p1->priority) 
            highP = p1;
        }
      }
      tempP = highP;
      int storeP = tempP->priority;
      c->proc = tempP;
      switchuvm(tempP);
      tempP->state = RUNNING;
      swtch(&(c->scheduler), tempP->context);
      switchkvm();
      c->proc = 0;
      highP = p;
      for (struct proc *p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++)
      {
        if (p1->state != RUNNABLE)
          continue;
        if (!highP)
        {
          highP = p;
        }
        else
        {
          if (highP->priority > p1->priority)
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
```

# part(c) {MLFQ} :-
    Logic :- The logic is simple loop over all the proc-stat and find the RUNNABLE 
             processes and move it to q0 ,i did this with all of the processes.On 
             running the processes I, first ran the processes present in the queue
             0. If a process needs then 1 tick to finish it is moved ,to next queue
             else executed there.This again happens with all the processes present
             on the queues. Now since i,have implemented aging (the ticks array = 
             [1,2,4,8,16] for decrement in the queue level i.e from high priority 
             to low priority) and if a process waits to much then it's moved up
             the priority queue (ticks array = [86, 85, 84, 83, 82] for 0,1,2,3,4 
             queue's respectively). Different functions are defined like push,pop 
             getfront,getback,to make it easy on working woth the queue. A process
             in each queue is given same priority [15,35,55,75,95] for 0,1,2,3,4 
             queue's respectively. For all the processes on the same queue Round
             Robin is used. After completion of ticks limit for that queue the 
             process is exited and again put back into the queue to execute.
             The code is made readable and understandable by even showing the 
             condition statement that were not required to be show. 

code :- 

```
void scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;
  struct proc *p;

  for (;;)
  {
    sti();
    struct proc *alottedP = 0;
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (!(p->state == RUNNABLE))
      {
        continue;
      }
      if (p->state == RUNNABLE && p > 0)
      {
        if (!(get_queue_inx(p) == -1)) 
        {
          if (!total_size_of_queues[get_queue_inx(p)])
          {
            push(get_queue_inx(p), p);
          }
        }
      }
    }
    for (int i = 0; i < 5; ++i) 
    {
      while (total_size_of_queues[i] > 0)
      {
        struct proc *p = getFront(i);

        if (!check_proc(p)) 
        {
          alottedP = p;
          break;
        }
        else 
        {
          popFront(i);
          p = 0;
        }
      }

      if (!alottedP)
      { 
        continue;
      }
      else
      {
        break;
      }
    }
    if (alottedP)
    {
      c->proc = alottedP;
      switchuvm(alottedP);

      popFront(get_queue_inx(alottedP));
      alottedP->latestQTime = ticks;
      alottedP->num_run += 1;
      if (!(alottedP->state == RUNNING))
      {
        alottedP->state = RUNNING;
      }

      swtch(&(c->scheduler), alottedP->context);

      int val_variable1 = ticks - alottedP->latestQTime;
      int procTcks = val_variable1;
      int queueinx_p = get_queue_inx(alottedP);
      int var_flag1 = alottedP->state == RUNNABLE && procTcks > 0;
      if ((alottedP->state == SLEEPING) || ((procTcks < (1 << queueinx_p)) && var_flag1))
      {
        dec_priority_func(alottedP, 1);
      }
      else if ((procTcks == (1 << queueinx_p)) && alottedP->state == RUNNABLE)
      {
        dec_priority_func(alottedP, 0);
      }

      switchkvm();
      c->proc = 0;
    }
    release(&ptable.lock);
  }
}
```

# Bonus :- 
  graphs :-

  **graph_with_cpu1** : 
           
      Graph of benchmark function with just one cpu, Command used :
```
      make qemu-nox SCHEDULER=MLFQ CPUS=1
```
           
      Explanation :
        In this graph a dip of queue is seen because of the difference
        of last executed tick and the current ticks exceeds a particular
        value (which is different for every queue) and because of this
        the process got promoted to high priority queue and got executed
        imidiately after the previous process completes its allowed tick 
        in the queue.

   **graph_with_cpu2** :

      Graph of benchmark function with 2 cpu's, Command used :
```
      make qemu-nox SCHEDULER=MLFQ CPUS=2
```
           
      Explanation :
        In this graph no promotion of processes are seen as threre are two
        cpu's to satisfy them hence there was less chance of being able to 
        cross the limit to get promoted hence smooth sailing graph.

  **graph_with_changed_ticks** :

      Graph of benchmark function with 1 cpu's, Command used :
```
      make qemu-nox SCHEDULER=MLFQ CPUS=1
```

      Explanation :
        Here I decreased the limit on number of ticks to get promoted to higher
        level queue and hence because of which such an incredible behavious is
        seen with processes being getting allocated between queue 1, 2 & 3 
        instead of being only in queue 4.
