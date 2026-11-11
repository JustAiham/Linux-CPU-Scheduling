# Linux-CPU-Scheduling
Exploring Operating system scheduling in Linux.

## Defining CPU Scheduling
The computer's CPU has a limited number of cores. Each core can executes one thread at time. Then how does it decide which thread gets CPU time?
**ANSWER**: CPU Scheduler
## CPU Scheduler
#### What does it do? 
1. It decides which runnable task should execute
2. When that task should execute
3. When the CPU should switch to another task.

#### Context switching
The process in which the CPU switches from one task to another

## Processeses, threads, and schedulding
- Process: A running program with its own virtual address space and resources
- Thread: An execution unit within a process
- Task: Linux's general representation of a scheduler execution entity.
#### Example:
 User opens chrome. Chrome creates a process, and that process contains multiple threads. Those threads may include a main thread for UI, thread for rendering webpages, and a thread for downloads.
 Linux decides which threads gets CPU time.

 ### How Linux Schedules CPU Time
 1. A thread becomes runnable
 2. It enters a pool 
 3. The schedule selects a task
 4. the CPU executes it
 5. An event occurs, such as thread blocking, yielding, or preemption.
 6. Scheduler may move on to another task.

 ### Thread blocking, sleeping, yielding, and premption
#### Thread blocking
Thread blocking happens when a thread cannot continue until something happens.
##### example
```
scanf("%d", &number); 
```
This program waits for the user to enter a number.
While waiting, the thread doesn't need the CPU, and so Linux puts it in a sleeping/blocked state and schedule another thread.
##### Other examples:
- Waiting for a file to be read from disk.
- Waiting for network data.
- Waiting for a mutex (lock) held by another thread.
#### Thread sleeping
Thread is voluntarily waiting, for a specific amount of time or an event.
##### example
```
sleep(5); // sleeps for 5 seconds
```
#### Thread Yielding
Yielding happens when a thread voluntarily gives up the CPU.
##### example
```
while (1) {
    do_work();
    sched_yield();
}
```
This program runs but is willing to give up the CPU to another eligible task.

#### Preemption
Linux takes the CPU away from a running thread by force.
#### example
[Preemption example](examples/preemption.c)

Compile and run with 
```
gcc preemption.c -o preemption -pthread
./preemption
```

Neither thread in this example have ```sleep()```,     ```sched_yield()```, or any operation that intentionally waits, so they are continuously runnable. Linux kernal decides how to distribute CPU time between them.

## Linux Scheduler Decisions
```
pthread_create(&A, NULL, threadA, NULL);
pthread_create(&B, NULL, threadB, NULL);
```
These calls request that Linux creates threads. Once they are runnable, the kernal scheduler decides when they execute.

### Static Inspection
Linux exposes scheduler implementation details through its kernal source code tracing interfaces.

### kernal version:
```
uname -r
```
### Scheduler source code
The scheduler implementation is located in kernal/sched/.

#### important files
- **core.c**: Core scheduling  logic, including_schedule() and context_switch().
- **fair.c**: Fair scheduleing implentation
-**rt.c**:  Real time schedule implementation.
- **deadline.c**: Deadline schedule implementation



