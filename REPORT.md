2019101021
Prince Singh Tomar

---
---

***All This results found with using only 1 cpu***

command : make qemu-nox SCHEDULER=<schedular> CPUS=1

## Report

*** Test function : dpro ***

code :-
```
#include "types.h"
#include "stat.h"
#include "user.h"

int main()
{
    volatile float a = 0, b = 1.43, c = 1.35;

    for (volatile int i = 0; i < 10000000; i++)
    {
        for (volatile int z = 0; z < 50; z++)
            a = (a * b + c);
    }
    
    exit();
}
```

command used :- $ time dpro

**for what wtime and rtime means check proc.c**

# RR :- 
 Three tests :--
    a) wtime = 1
       rtime = 2034

    b) wtime = 0
       rtime = 2053

    c) wtime = 1
       rtime = 2022

    Avg :- wtime = 0.66 ticks
           rtime = 2036.66 ticks

# FCFS :-
 Three tests :--
    a) wtime = 1
       rtime = 2057

    b) wtime = 1
       rtime = 2292

    c) wtime = 0
       rtime = 2237

    Avg :- wtime = 0.66 ticks
           rtime = 2195.33 ticks

# PBS :- 
 Three tests :--
    a) wtime = 1
       rtime = 2299

    b) wtime = 0
       rtime = 2038

    c) wtime = 0
       rtime = 2103

    Avg :- wtime = 0.33 ticks
           rtime = 2146.66 ticks

# MLFQ :- 
 Three tests :--
    a) wtime = 0
       rtime = 2053

    b) wtime = 1
       rtime = 2188

    c) wtime = 0
       rtime = 2288

    Avg :- wtime = 0.3 ticks
           rtime = 2176.33 ticks

waiting time is less because dpro function is cpu intensive function 
and time function just waits for process to over hence small wtime.

Here we can say that RR > PBS > MLFQ > FCFS

RR comes out to be the fastest for such kind of processes.

Taking PBS is a bit wrong since the priority of processes is same hence
its just RR with priority checking,hence a bit slow as compared to RR.
Refer README.md for more information on PBS implementation.

MLFQ is 3rd fastest since the processes that need less time to execute are 
excecuted imidiately leaving the heavy processes which are also implemented
according to the defined procedure.

FCFS comes out to be the slowest.

*** Test function : benchmark ***

command :- time benchmark

code :- 
```
#include "types.h"
#include "user.h"

int number_of_processes = 10;

int main(int argc, char *argv[])
{
  int j;
  for (j = 0; j < number_of_processes; j++)
  {
    int pid = fork();
    if (pid < 0)
    {
      printf(1, "Fork failed\n");
      continue;
    }
    if (pid == 0)
    {
      volatile int i;
      for (volatile int k = 0; k < number_of_processes; k++)
      {
        if (k <= j)
        {
          sleep(200); //io time
        }
        else
        {
          for (i = 0; i < 100000000; i++)
          {
            ; //cpu time
          }
        }
      }
       printf(1, "Process: %d Finished\n", j);
      exit();
    }
    else{
        ;
       //chpr(100-(20+j),pid);            //commented out for PBS
       // will only matter for PBS
       //comment it out if not implemented yet (better priorty for 
       //more IO intensive jobs)
    }
  }
  for (j = 0; j < number_of_processes+5; j++)
  {
    wait();
  }
  exit();
}

```
# MLFQ :-

Test 1 :
   wtime = 2037
   rtime = 5

Test 2 :
   wtime = 2042
   rtime = 5

wtime_avg = 2039.5
rtime_avg = 5

In these times the time of child process is not counted

Total time = wtime_avg + rtime_avg = etime - stime = 2044.5 

# FCFS :-

Test 1 :
   wtime = 3599
   rtime = 3

Test 2 :
   wtime = 3600
   rtime = 4

wtime_avg = 3599.5
rtime_avg = 3.5

In these times the time of child process is not counted

Total time = wtime_avg + rtime_avg = etime - stime = 3603

# PBS :-

Test 1 :
   wtime = 2011
   rtime = 3

Test 2 :
   wtime = 2011
   rtime = 3

wtime_avg = 2011
rtime_avg = 3

In these times the time of child process is not counted

Total time = wtime_avg + rtime_avg = etime - stime = 2014

# RR :-

Test 1 :
   wtime = 2013
   rtime = 4

Test 2 :
   wtime = 2014
   rtime = 5

wtime_avg = 2013.5
rtime_avg = 4.5

In these times the time of child process is not counted

Total time = wtime_avg + rtime_avg = etime - stime = 2019

here we can say that 
   PBS > RR > MLFQ > FCFS

Hence most of the time the parent was just sleeping and the childs 
were doing the work or waiting hence Total time was used to approx.
estimate the behaviour and effectiveness.

The above two process tests shows that FCFS is the slowest and (RR 
and PBS) are the fastest MLFQ is in midway between them


**Ans of the question in the pdf** :-

If the process yields the CPU voluntarily, then when the process becomes 
RUNNABLE it would automatically be in the same queue (c_queue does not change)
Or simply the
ans is that if a process voluntarly vacates the CPU then there would be faster 
reallocation its because it will be in the same queue and thus will have 
higher priority and thus the reallocation would definitely be faster.

