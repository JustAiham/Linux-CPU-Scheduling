# Linux-CPU-Scheduling

Exploring operating system scheduling in Linux.

# 1. Fundamentals

## Defining CPU Scheduling

The computer's CPU has a limited number of cores. Each core (or logical CPU) executes one thread at a time. Then how does it decide which thread gets CPU time?

**ANSWER:** CPU Scheduler

## CPU Scheduler

#### What does it do?

1. It decides which runnable task should execute.
2. When that task should execute.
3. When the CPU should switch to another task.

#### Context switching

The process in which the CPU switches from one task to another.

## Processes, Threads, and Scheduling

- Process: A running program with its own virtual address space and resources.
- Thread: An execution unit within a process.
- Task: Linux's general representation of a scheduler execution entity.

#### Example

User opens Chrome. Chrome creates a process, and that process contains multiple threads. Those threads may include a main thread for UI, a thread for rendering webpages, and a thread for downloads.

Linux decides which threads get CPU time.

### How Linux Schedules CPU Time

1. A thread becomes runnable.
2. It enters a pool.
3. The scheduler selects a task.
4. The CPU executes it.
5. An event occurs, such as thread blocking, yielding, or preemption.
6. The scheduler may move on to another task.

### Thread Blocking, Sleeping, Yielding, and Preemption

#### Thread blocking

Thread blocking happens when a thread cannot continue until something happens.

##### Example

```c
scanf("%d", &number);
```

This program waits for the user to enter a number. While waiting, the thread doesn't need the CPU, and so Linux puts it in a sleeping/blocked state and schedules another thread.

##### Other examples

- Waiting for a file to be read from disk.
- Waiting for network data.
- Waiting for a mutex (lock) held by another thread.

#### Thread sleeping

A thread is voluntarily waiting for a specific amount of time or an event.

##### Example

```c
sleep(5); // sleeps for 5 seconds
```

#### Thread yielding

Yielding happens when a thread voluntarily gives up the CPU.

##### Example

```c
while (1) {
    do_work();
    sched_yield();
}
```

This program runs but is willing to give up the CPU to another eligible task.

#### Preemption

Linux takes the CPU away from a running thread.

##### Example

[Preemption example](examples/preemption.c)

Compile and run with:

```bash
gcc preemption.c -o preemption -pthread
./preemption
```

Neither thread in this example has `sleep()`, `sched_yield()`, or any operation that intentionally waits, so they are continuously runnable. The Linux kernel decides how to distribute CPU time between them.

## Linux Scheduler Decisions

From [Preemption example](examples/preemption.c):

```c
pthread_create(&A, NULL, threadA, NULL);
pthread_create(&B, NULL, threadB, NULL);
```

These calls request that Linux creates threads. Once they are runnable, the kernel scheduler decides when they execute.

---

# 2. Investigating the Linux Scheduler

There are two ways to investigate the Linux scheduler:

- **Static Inspection:** Reading the kernel source code to understand how scheduling is implemented.
- **Dynamic Inspection:** Observing scheduler events while the kernel is running.

## Static Inspection

Static inspection involves examining Linux kernel source code to understand how scheduling is implemented.

### 1. Locate Kernel Headers
```bash
uname -r
ls /usr/src/
```
Look for the source directory matching your running kernel.
### 2. Find your system
ls /usr/src/[your system's kernel source]
#### Example
input 
```bash
uname -r
ls /usr/src/
```
output:
```
7.0.0-43-generic
linux-headers-6.14.0-37-generic   linux-hwe-6.17-headers-6.17.0-35
linux-headers-6.17.0-35-generic   linux-hwe-6.17-headers-6.17.0-40
linux-headers-6.17.0-40-generic   linux-hwe-7.0-headers-7.0.0-28
linux-headers-7.0.0-28-generic    linux-hwe-7.0-headers-7.0.0-29
linux-headers-7.0.0-29-generic    linux-hwe-7.0-headers-7.0.0-31
linux-headers-7.0.0-31-generic    linux-hwe-7.0-headers-7.0.0-34
linux-headers-7.0.0-34-generic    linux-hwe-7.0-headers-7.0.0-38
linux-headers-7.0.0-38-generic    linux-hwe-7.0-headers-7.0.0-43
linux-headers-7.0.0-43-generic    nvidia-595.99.02
linux-hwe-6.14-headers-6.14.0-37  python3.12
```
To find this system source:
```bash
ls /usr/src/linux-hwe-7.0-headers-7.0.0-43/
```
### What we are looking for

Once we locate the relevant source files, we'll find these definitions:

| Structure | Responsibility |
|:---|:---|
| `task_struct` | Stores information about an individual task |
| `rq` | Maintains scheduling information for a CPU |
| `sched_class` | Defines how a scheduling class selects tasks |

### . Find the Definitions

Use `grep` to locate each structure in the kernel headers:

**NOTE: Replace the system source with your specific system**
```bash
# Find task_struct
grep -n "struct task_struct {" /usr/src/linux-hwe-7.0-headers-7.0.0-43/include/linux/sched.h

# Find rq
grep -n "struct rq {" /usr/src/linux-hwe-7.0-headers-7.0.0-43/kernel/sched/sched.h

# Find sched_class
grep -n "struct sched_class {" /usr/src/linux-hwe-7.0-headers-7.0.0-43/include/linux/sched.h
```
## 3. Issue: Missing Fileds
### struct rq and sched_class not found
It seems the system doesn't contain all the kernel definition that we need to inspect.
## **To solve this**
1. #### Identify the kernel source package and version
```bash
dpkg-query -W -f='Source package: ${source:Package}\nSource version: ${source:Version}\n' linux-image-$(uname -r)
```
This gave us:
```
    Source package: linux-signed-hwe-7.0

    Source version: 7.0.0-43.43~24.04.1
```
2. **Enable source repositories**

APT was configured to retrieve binary packages (deb), but not source packages (deb-src).
Edit ```/etc/apt/sources.list.d/official-package-repositories.list``` and add a deb-src for each Ubuntu repository.
```bash
nano /etc/apt/sources.list.d/official-package-repositories.list
```
#### Before and after configuration
[Before adding deb-src](./kernelFiles/enableSourceRepos/beforeSourceRepoConfig.list)

[After adding deb-src](./kernelFiles/enableSourceRepos/afterSourceRepoConfig.list)


3. Update package information
```bash
sudo apt update
```
4. Download the matching kernel source
```bash
apt source [your version]
```
paste your [kernel's version](#identify-the-kernel-source-package-and-version) above

#### example
```bash
apt source linux-hwe-7.0=7.0.0-43.43~24.04.1
```
This downloads and extracts the full Linux kernel source, including Ubuntu's patches.
We are able to move on to step 4 and inspect the scheduler

### Scheduler Source Code

The scheduler implementation is located in `kernel/sched/` inside the package that you downloaded.

``` bash
/linux-hwe-7.0-7.0.0/kernel/sched
```

#### Important Files

- **core.c:** Core scheduling logic, including `__schedule()` and `context_switch()`.
- **fair.c:** Fair scheduling implementation.
- **rt.c:** Real-time scheduling implementation.
- **deadline.c:** Deadline scheduling implementation.
### 4. Locate the Scheduling Logic

Find `__schedule()` in `core.c`:

```bash
grep -n "__schedule(" /usr/src/linux-hwe-7.0-headers-7.0.0-43/kernel/sched/core.c
```
This identifies references to __schedule(), including its actual function definition.


We are interested in these lines:
```
6755: * __schedule() is the main scheduler function.
6793:static void __sched notrace __schedule(int sched_mode)

```
The function definition is located at line 6793 in our downloaded source.
```bash
6793: static void __sched notrace __schedule(int sched_mode)
```
### 6. static void __sched notrace __schedule(int sched_mode)

#### Inpsect the function Part 1
```sed -n '6755,6870p' core.c```

We want to inspect the function's introductory comments, declaration, and initial implementation without displaying the entire source file.


#### Observation
The [introductory comments](./kernelFiles/sched_files/sched_corel_lines6755_6870.txt) explain three ways the scheduler is entered:

- Explicit blocking, such as waiting on a mutex or semaphore.

- Preemption, when the current task needs to give up the CPU.

- Wakeups, which make tasks runnable and may trigger preemption.

**Important Variables**

Variable | Purpose
|:----- | ----:|
|prev | Currently running task|
|next |Task selected to run next|
|rq | Runqueue for the current CPU|
|cpu | ID of the current CPU|
|rf | Stores runqueue locking information|

Both prev and next are pointers to struct task_struct.

#### Identifying the Current CPU and Task
```bash
cpu = smp_processor_id();
rq = cpu_rq(cpu);
prev = rq->curr;
```
cpu = smp_processor_id();
rq = cpu_rq(cpu);
prev = rq->curr;

- smp_processor_id() identifies the CPU currently executing the code.
- cpu_rq(cpu) retrieves that CPU's runqueue.
- rq->curr retrieves the task currently executing on that CPU.

Linux maintains a separate runqueue for each CPU.

#### Protecting the Runqueue
Before making scheduling decisions, the scheduler protects the runqueue:
```
local_irq_disable();
rq_lock(rq, &rf);
smp_mb__after_spinlock();
```
- local_irq_disable() disables local interrupts.

- rq_lock() acquires the runqueue lock.

- smp_mb__after_spinlock() provides memory-ordering guarantees.

This protection helps prevent concurrent operations from interfering with scheduling decisions.

### 7. How the scheduler selects next tasks
To see the scheduling in ```core.c```, we need need to inspect the next portion of the function.
#### Inspecting the function part 2
```sed -n '6870,6960p' core.c```

Check [sched_core_lines file](./kernelFiles/sched_files/sched_core_lines_6870_6960.txt) to see full output

Detailed _schedule() analysis is moved to [Inside Linux Scheduler section](#3-inside-the-linux-scheduler).

#### Observation
__schedule() coordinates task state handling, task selection, and context switching. The actual selection is delegated to pick_next_task(), while the transition between tasks is handled by context_switch().

## Dynamic Inspection

Linux exposes scheduler events through ftrace, a kernel tracing framework.

### Available Scheduler Events

```bash
sudo ls /sys/kernel/tracing/events/sched/
```

#### Important Events

- **sched_switch:** Records when the CPU switches from one task to another.
- **sched_wakeup:** Records when a sleeping task becomes runnable.
- **sched_wakeup_new:** Records when a newly created task becomes runnable.
- **sched_migrate_task:** Records when a task migrates between CPUs.

### Inspecting sched_switch

```bash
sudo cat /sys/kernel/tracing/events/sched/sched_switch/format
```

#### Enable sched_switch

To see individual events:

```bash
sudo sh -c 'echo 1 > /sys/kernel/tracing/events/sched/sched_switch/enable'
```

This tells Linux to record every `sched_switch` event.

#### Read the Trace

```bash
sudo cat /sys/kernel/tracing/trace
```

You should see entries resembling:

```text
worker-1234 [002] ... sched_switch:
prev_comm=worker prev_pid=1234 prev_state=R ==> next_comm=chrome next_pid=5678
```

#### Understanding the Output

| Field | Meaning |
|---|---|
| `prev_comm` | Previous task |
| `prev_pid` | Previous task's PID |
| `prev_state` | Previous task's state |
| `next_comm` | Next task |
| `next_pid` | Next task's PID |

`==>` separates the task being switched out from the task being switched in.

### Observing Scheduler Activity in Real Time

Make sure `sched_switch` is enabled.

#### Generate CPU Activity

In another terminal, run:

```bash
yes > /dev/null
```

This creates a continuously runnable process that consumes CPU time.

#### Use trace_pipe to Stream Scheduler Events

```bash
sudo cat /sys/kernel/tracing/trace_pipe
```

To filter the trace output and find scheduling events involving `yes`:

```bash
sudo cat /sys/kernel/tracing/trace_pipe | grep --line-buffered "yes"
```

Now you should see scheduling events involving the `yes` process.

Stop `yes` with `Ctrl+C`.

#### Observations

- The `yes` process continuously consumes CPU time.
- Linux schedules the process alongside other runnable tasks.
- `sched_switch` records when the process is switched in or out.
- The `prev_comm` and `next_comm` fields identify the tasks involved in each context switch.

### Inspecting sched_wakeup

`sched_wakeup` records when a task becomes runnable.

#### Wakeup vs. Context Switching

**Wakeup:** Happens when a task becomes eligible to run.

**Context switch:** Happens when the CPU starts executing a different task.

A task can become runnable and still wait before receiving CPU time.

#### 1. Enable the Tracepoints

Enable both `sched_wakeup` and `sched_switch`:

```bash
sudo sh -c '
echo 1 > /sys/kernel/tracing/events/sched/sched_switch/enable
echo 1 > /sys/kernel/tracing/events/sched/sched_wakeup/enable
'
```

#### 2. Start Live Tracing

```bash
sudo cat /sys/kernel/tracing/trace_pipe | grep --line-buffered "sleep"
```

This is filtered so that we only see events containing `sleep`.

#### 3. Generate Sleeping and Waking Activity

```bash
while true; do sleep 1; done
```

Let it run for approximately 5 seconds, then stop with `Ctrl+C`.

#### Observations

1. `sleep` waits for its timer.
2. Its timer expires.
3. Linux makes `sleep` runnable again.
4. The scheduler eventually gives it CPU time.
5. `sleep` finishes, allowing the shell to continue.

The `sched_wakeup` event records the transition to runnable. It does not guarantee immediate execution.

#### 4. Inspect the Event Format

```bash
sudo cat /sys/kernel/tracing/events/sched/sched_wakeup/format
```

#### Understanding sched_wakeup Fields

| Field | Meaning |
|---|---|
| `common_type` | Internal event identifier |
| `common_flags` | Flags describing the event context |
| `common_preempt_count` | Preemption nesting count |
| `common_pid` | PID associated with the task context in which the event was recorded |
| `comm` | Name of the task being awakened |
| `pid` | PID of the task being awakened |
| `prio` | Scheduling priority of the awakened task |
| `target_cpu` | CPU targeted for the awakened task |

The `print fmt` section tells the tracing framework how to format the raw event fields into readable output.

For example:

```text
comm=sleep pid=1234 prio=120 target_cpu=003
```

### Understanding target_cpu

What interests us most is `target_cpu`.

It tells us which CPU Linux is targeting for the awakened task. This becomes particularly interesting when we study multicore scheduling.

### Disable Tracing

After finishing, disable the tracepoints:

```bash
sudo sh -c '
echo 0 > /sys/kernel/tracing/events/sched/sched_switch/enable
echo 0 > /sys/kernel/tracing/events/sched/sched_wakeup/enable
'
```

---

# 3. Inside the Linux Scheduler

This section will cover the actual scheduler implementation, including:

- `__schedule()`
- Runqueues
- Scheduling classes
- Context switching
- Multicore scheduling
## __schedule()

### Identifying the Current Task

Before selecting another task, `__schedule()` identifies the CPU
currently executing it and retrieves that CPU's runqueue.

- `smp_processor_id()`: Gets the ID of the current CPU.
- `cpu_rq(cpu)`: Retrieves that CPU's runqueue.
- `rq->curr`: Identifies the task currently running on that CPU.

### How the scheduler works
- task_struct: Represents a task (process or thread) in the kernel.
- rq: The runqueue, which tracks runnable tasks for a CPU.

#### Handling the Previous Task
```
try_to_block_task(rq, prev, &prev_state,
                  !task_is_blocked(prev));
switch_count = &prev->nvcsw;
```
The scheduler handles the previous task's blocking state. It also points switch_count to prev->nvcsw, which tracks voluntary context switches.

#### Selecting the Next Task
```
next = pick_next_task(rq, &rf);
rq_set_donor(rq, next);
rq->next_class = next->sched_class;
```
pick_next_task() selects the next runnable task according to the [scheduling classes](#scheduling-classes).

The scheduler also updates runqueue information associated with the selected task and its scheduling class.

#### Determining Whether to Switch
```
is_switch = prev != next;
```
The scheduler compares the previous and next tasks.

- If they are different, a context switch is necessary.

- If they are the same, the current task continues running.

#### Performing the Context Switch
```
trace_sched_switch(preempt, prev, next, prev_state);
rq = context_switch(rq, prev, next, &rf);
```
The scheduler records the scheduling event and calls context_switch() to transition between tasks.

If no switch is necessary, the scheduler instead releases the runqueue lock.
## Runqueues

A runqueue (rq) is a per-CPU data structure that tracks the tasks available to run on that CPU.

A few important points:
- Each CPU has its own runqueue.
- The runqueue tracks the currently running task (rq->curr).
- It maintains scheduling information for runnable tasks.
- The scheduler uses it when selecting the next task.

### Inspect the implementation
From kernel/sched/, run:

```
grep -n "struct rq {" sched.h
```

output:

```
1123:struct rq {

```

#### Inspect the definition using the line returned
```
sed -n '1100,1200p' sched.h
```

full output in [sched.h](./kernelFiles/sched_files/sched.h)

We are interested in `struct rq`, which is the main per-CPU runqueue data structure.

##### Understanding fields
|Field |	Purpose|
|:-----| -----:|
|nr_running	|Number of runnable tasks on this CPU.
|curr	|Currently executing task.
|donor	|Scheduling context, relevant to proxy execution.
|idle	|Idle task used when no regular task is available to run.
|nr_switches	|Number of context switches on this CPU.
|__lock	|Lock protecting runqueue operations.
|cfs	|Runqueue data for fair scheduling.
|rt	| Runqueue data for real-time scheduling.
|dl	| Runqueue data for deadline scheduling.
|scx	|Runqueue data for sched_ext, when enabled.

#### Important distinction for runqueue
- rq is the main runqueue for a CPU.
- cfs_rq, rt_rq, and dl_rq maintain scheduling information for their respective scheduling classes.

### Observation
The runqueue is the central data structure used by the scheduler on each CPU.

It maintains information about runnable tasks and contains separate runqueue structures for different scheduling classes.

This allows Linux to manage tasks according to their scheduling policies while keeping the main runqueue organized per CPU.

The `__schedule()` function retrieves the current CPU's runqueue using `cpu_rq(cpu)` and uses it to select the next task.

## Scheduling Classes
A **scheduling** class defines how the scheduler selects and manages tasks with a particular scheduling policy.

Linux uses different scheduling classes because not every task has the same requirements.

#### Important scheduling classes

Class	| Purpose
|:-----| -----:|
|Fair (fair) |	Shares CPU time among ordinary tasks.
|Real-time (rt) |	Prioritizes time-sensitive tasks.
|Deadline (dl) |	Schedules tasks according to timing constraints and deadlines.

### Inspect Implementation

```
grep -n "DEFINE_SCHED_CLASS" fair.c rt.c deadline.c idle.c
```
This searches for the definitions of the scheduling classes in their respective source files.

output:

```
fair.c:14077:DEFINE_SCHED_CLASS(fair) = {
rt.c:2597:DEFINE_SCHED_CLASS(rt) = {
deadline.c:3436:DEFINE_SCHED_CLASS(dl) = {
idle.c:569:DEFINE_SCHED_CLASS(idle) = {

```

#### Inspect each scheduling class
** `fair.c`**
```
sed -n '14065,14115p' fair.c
```
fair.c output is located in the [scheduling_classes/fair.c](./kernelFiles/scheduling_classes/fair.c)

#### Importation pointers
|Function pointer|	Responsibility
|:-------| -----------:|
|.enqueue_task	|Adds a task to the fair runqueue.
|.dequeue_task	|Removes a task from the fair runqueue.
|.pick_next_task	|Selects the next task to execute.
|.put_prev_task	|Handles the previously running task.
|.set_next_task	|Prepares the selected task to run.
|.wakeup_preempt	|Checks whether a waking task should preempt the current task.
|.task_tick	|Handles scheduler tick events.
|.select_task_rq	|Selects a CPU for a task.

#### connecting back to __schedule()

```
next = pick_next_task(rq, &rf);
```
The scheduler uses pick_next_task() to select the next task. The scheduling class provides the class-specific implementation of that selection through .pick_next_task.

#### Additional note
Linux uses function pointers here instead of putting all the logic inside `__schedule()` because of 3 reason:

**`__schedule()`**
- Abstraction: doesn't need to know the internal implementations of every scheduling policy.
- Encapuslation: Each scheduling class keeps its specific scheduling behavior in its own implementation, such as fair.c, rtc.c, deadline.c.
- Polymorphism: the scheduler calls the same operation (e.g., pick_next_task()) while actual implementation depends on the scheduling class.

 
** `rt.c`**
```
sed -n '2585,2635p' rt.c
```

output is located in the [scheduling_classes/rt.c](./kernelFiles/scheduling_classes/rt.c)

#### Important Pointers
|Function pointer	|Responsibility
|:----------| ----------:|
|.enqueue_task	|Adds a task to the real-time runqueue.
|.dequeue_task	|Removes a task from the real-time runqueue.
|.pick_task	|Selects a real-time task canidate.
|.put_prev_task	|Handles the previously running task.
|.set_next_task	|Prepares the selected task to run.
|.wakeup_preempt	|Determines whether a waking task should preempt the current task.
|.balance	|Handles balancing real-time tasks across CPUs.
|.task_tick	|Handles scheduler tick events.
|.get_rr_interval	|Gets the time quantum for round-robin scheduling.

#### fair scheduling vs Real time scheduling

**There are two important differences:**

- **Priority:** Real-time tasks use real-time priorities to determine which task should execute first.
- **Scheduling policies:** Linux supports SCHED_FIFO and SCHED_RR for real-time tasks.

#### SCHED_FIFO: 
A task runs until it blocks, yields, or is preempted by a higher-priority task.

#### SCHED_RR: 
Similar to FIFO, but tasks at the same priority take turns using a time quantum.

#### Additional note
Real-time scheduling doesn't guarantee that a task will always meet its deadline. It provides scheduling behavior designed for time-sensitive tasks.


** `deadline.c`**
```
sed -n '3425,3485p' deadline.c
```
#### Important Pointers
|Function pointer|	Responsibility
|:------| ------:|
|.enqueue_task	|Adds a task to the deadline runqueue.
|.dequeue_task	|Removes a task from the deadline runqueue.
|.pick_task	|Selects a deadline task candidate.
|.put_prev_task	|Handles the previously running task.
|.set_next_task	|Prepares the selected task to run.
|.wakeup_preempt	|Determines whether a waking task should preempt the current task.
|.balance	|Handles balancing deadline tasks across CPUs.
|.task_tick	|Handles scheduler tick events.
|.update_curr	|Updates execution-time accounting for the current task.

We are interested most in .get_prio, which provides priority information for deadline tasks.

#### How deadline scheduling works
Linux uses Earliest Deadline First (EDF) for deadline scheduling.

A deadline task is configured with three important timing parameters:
- Runtime (R): How much CPU execution time the task needs.
- Deadline (D): The relative time by which the task should finish its execution.
- Period (P): How frequently the task is expected to execute.

#### example

A task might require 10 ms of CPU time every 50 ms, with a relative deadline of 40 ms.



## Comparing the schedluing classes
|Class|	Scheduling approach
|:-----| ----:|
|Fair	|Distributes CPU time fairly.
|Real-time	|Prioritizes tasks by real-time priority.
|Deadline	|Prioritizes tasks by earliest deadline.

### additional note
Deadline scheduling is designed to meet timing requirements, but it does not guarantee every deadline will be met under all system conditions.

#### If a task doesn't meet the deadline
for Linux's `SCHED_DEADLINE`:
- Runtime is exhausted: If a task consumes its allocated runtime before its scheduling period ends, it gets throttled.
- Task is throttled: It stops receiving CPU time under that deadline reservation.
- Replenishment: When its next period begins, its runtime budget is replenished, allowing it to execute again.

###### example
Suppose a task has:
- Runtime: 10 ms
- Deadline: 40 ms
- Period: 50 ms
If it needs 12 ms of CPU time to finish:
- It consumes its 10 ms runtime budget.
- Linux throttles it because its budget is exhausted.
- It cannot finish within its allocated runtime.
- It becomes eligible to execute again when its runtime is replenished.

## Context switching
A context switch occurs when the CPU stops executing one task and starts executing another.

The kernel must preserve the previous task's execution state and restore the next task's state so both can continue correctly.

We now know this implementation from `__schedule()`

```
rq = context_switch(rq, prev, next, &rf);
```
### Inspect context_switch implementation
Find the location of the function definition in core.c

```
grep -n "context_switch(struct rq" core.c
```
Find the full definition
```
sed -n '5264,5345p' core.c
```
Full ouput is located in [kernelFiles/context_switch.c](./kernelFiles/contextSwitching/context_switch.c)
### Stages of context_switch

#### 1. Prepare the switch

```
prepare_task_switch(rq, prev, next);
arch_start_context_switch(prev);
```
- prev: the task currently running.
- next: the task selected to run next.
- prepare_task_switch(): performs the necessary preparation before switching.
- arch_start_context_switch(): handles architecture-specific preparation.

#### 2. Handle memory address spaces

```
if (!next->mm) {
    enter_lazy_tlb(prev->active_mm, next);
    ...
} else {
    switch_mm_irqs_off(prev->active_mm, next->mm, next);
    ...
}
```
This part handles the memory context of the incoming task.
- mm: represents a task's user-space memory address space.
- active_mm: tracks the active memory context, including for kernel threads.
- switch_mm_irqs_off(): switches to the incoming task's memory address space when necessary.

#### 3. Switch execution contexts
```
prepare_lock_switch(rq, next, rf);

/* Here we just switch the register state and the stack. */
switch_to(prev, next, prev);
barrier();

return finish_task_switch(prev);
```
This is the actual execution-context switch.
- prepare_lock_switch(): prepares runqueue lock handling.
- switch_to(): switches the CPU's execution context, including register state and stack.
- barrier(): prevents certain compiler reordering around the switch.
- finish_task_switch(): completes the switch and handles cleanup associated with the previous task.

#### Additional note
switch_to() is where the low-level switch happens. context_switch() handles the preparation, memory context, and cleanup around it.


## Multicore scheduling
For this section, we'll focus on 3 aspects:

- **CPU selection**: How does Linux decide which CPU should execute a task?
- **Load balancing**: How does Linux distribute tasks across CPUs?
- **Task migration**: How does Linux move a task from one CPU to another?


### CPU Selection
CPU selection determines the target CPU for a task. Linux considers factors such as CPU affinity, the task's previous CPU, CPU availability, and workload distribution.

### Find function definition

```
grep -n "select_task_rq_fair" fair.c
```

output

```
8151:		 * select_task_rq_fair() races with LB's detach_task().
8677: * select_task_rq_fair: Select target runqueue for the waking task in domains
8687:select_task_rq_fair(struct task_struct *p, int prev_cpu, int wake_flags)
14090:	.select_task_rq		= select_task_rq_fair,
```
We are interested in select_task_rq_fair() because it implements how the fair scheduling class chooses a target CPU for a waking task.

### Inspect implementation
From `kernel/sched/`, run:
```
sed -n '8670,8770p' fair.c
```
Full output is located in [kernelFiles/Multicore Scheduling/CPU_selection](./kernelFiles/multicore%20Scheduling/CPU_selection.c)

### Understanding `select_task_rq_fair()`

#### A. Previous CPU
```
int new_cpu = prev_cpu;
```

Linux initially considers the CPU where the task ran previously. Keeping a task on the same CPU can help preserve cache locality.

#### B. CPU affinity
```
cpumask_test_cpu(cpu, p->cpus_ptr)
```

Checks whether a CPU is allowed to execute the task.

A task's CPU affinity restricts which CPUs it can run on.

#### C. Energy-efficient CPU selection
```
new_cpu = find_energy_efficient_cpu(p, prev_cpu);
```

When the scheduling conditions allow it, Linux attempts to find an energy-efficient CPU for the task.

#### D. Idle CPU selection and load balancing
```
new_cpu = sched_balance_find_dst_cpu(sd, p, cpu, prev_cpu, sd_flag);
```
This is the slower path, which searches for a destination CPU using scheduling-domain balancing.

##### Alternatively
```
new_cpu = select_idle_sibling(p, prev_cpu, new_cpu);
```
This attempts to find an idle CPU among suitable sibling CPUs.

### Section summary
- Linux considers the task's previous CPU.
- CPU affinity restricts the available CPUs.
- Linux can consider energy efficiency.
- Linux can select an idle CPU or use load-balancing logic to find a destination.

### Additional note
CPU selection and task migration are related, but not identical. CPU selection determines where a task should run, while task migration handles the movement of a task between CPUs.

### Load Balancing

Load balancing is the process of distributing tasks across CPUs to prevent some CPUs from being overloaded while others remain underutilized.

Linux uses scheduling domains to organize CPUs and determine where load balancing should occur.

### Find function definition

From `kernel/sched/`, run:

```bash
grep -nE "load_balance|rebalance_domains|sched_balance" fair.c core.c
```

The search identified the following functions in `fair.c`:

- `sched_balance_rq()` (found at line 11991)
- `sched_balance_domains()` (found at line 12457)

We are most interested in `sched_balance_rq()`, which performs the actual balancing work.

### Inspect its implementation:

```bash
sed -n '11985,12110p' fair.c
```

Full output is located in [sched_balance_rq.c](./kernelFiles/multicore%20Scheduling/sched_balance_rq()).

### Understanding sched_balance_rq()

`sched_balance_rq()` checks whether a CPU is balanced within its scheduling domain and attempts to move tasks when an imbalance is detected.

#### Important steps

**1. Check whether balancing is needed**

```c
if (!should_we_balance(&env)) {
    *continue_balancing = 0;
    goto out_balanced;
}
```

Checks whether the current CPU should perform load balancing.

**2. Find a busy scheduling group and CPU**

```c
group = sched_balance_find_src_group(&env);
busiest = sched_balance_find_src_rq(&env, group);
```

- `sched_balance_find_src_group()`: finds a busy scheduling group.
- `sched_balance_find_src_rq()`: identifies the busiest runqueue within that group.

**3. Detach tasks from the busy CPU**

```c
cur_ld_moved = detach_tasks(&env);
```

Selects and detaches movable tasks from the source runqueue.

**4. Attach tasks to the destination CPU**

```c
if (cur_ld_moved) {
    attach_tasks(&env);
    ld_moved += cur_ld_moved;
}
```

Attaches the detached tasks to the destination runqueue.

#### Observation

Linux load balancing attempts to distribute work across CPUs when scheduling domains become imbalanced.

The general process is:
1. Determine whether balancing is needed.
2. Find a busy scheduling group and runqueue.
3. Detach movable tasks from the busy runqueue.
4. Attach those tasks to another CPU's runqueue.

This allows Linux to distribute workloads across available CPUs instead of leaving work concentrated on a single CPU.


### Task Migration
Task migration is the process of moving a task from one CPU's runqueue to another CPU's runqueue.

Linux may migrate tasks to:
- Balance workloads across CPUs.
- Move a task to a more suitable CPU.
- Respond to CPU availability or affinity constraints.

We are interested in 
```
migrate_task_rq_fair(struct task_struct *p, int new_cpu)
```
### Find definition
from `kernel/sched`, run:

```
grep -n "migrate_task_rq_fair" fair.c
```

output:

```
grep -n "migrate_task_rq_fair" fair.c
4952:		 * migrate_task_rq_fair() indicating we migrated.
8761:static void migrate_task_rq_fair(struct task_struct *p, int new_cpu)
14091:	.migrate_task_rq	= migrate_task_rq_fair,
```
We are interested in lines 8761.

### Inspect its implementation
```
sed -n '8755,8835p' fair.c
```

Full output  is located in [migrate_task_rq.c](./kernelFiles/Task%20Migration/migrate_task_rq.c)

### Understanding migrate_task_rq_fair()

This function is called immediately before a task is migrated to another CPU. It updates scheduling information associated with the task.

#### Important steps

##### 1. Access the task's scheduling entity
```
struct sched_entity *se = &p->se;
```

Retrieves the task's scheduling entity, which stores scheduling information used by the fair scheduler.

##### 2. Update load tracking
```
remove_entity_load_avg(se);
migrate_se_pelt_lag(se);
```

`remove_entity_load_avg():` removes the task's load contribution from the previous runqueue.

`migrate_se_pelt_lag():` adjusts the task's PELT information to maintain continuity during migration.

##### 3. Reset the timestamp
```
se->avg.last_update_time = 0;
```
Resets the last-update timestamp so scheduling statistics can be updated appropriately on the new CPU.

##### 4. Update the scan period

```
update_scan_period(p, new_cpu);
```

Updates the task's scan period based on its destination CPU.

#### Observation

Task migration involves more than simply moving a task between CPUs. The scheduler must also maintain and update scheduling information so load tracking remains consistent after migration.

## Conclusion

Through this investigation, we explored how the Linux scheduler manages tasks across single-core and multicore systems.

We examined how `__schedule()` coordinates task selection, how per-CPU runqueues organize runnable tasks, and how scheduling classes implement different scheduling policies.

We also investigated how context switching allows the CPU to move between tasks and how multicore scheduling distributes workloads through CPU selection, load balancing, and task migration.

Overall, the Linux scheduler is not a single algorithm. It is a collection of coordinated mechanisms and scheduling classes that work together to manage CPU resources, prioritize tasks, and maintain system responsiveness and efficiency.

By inspecting the kernel source code, we gained a better understanding of how these mechanisms are implemented and how they interact within the Linux kernel.

#### Youtube Link:
[Linux CPU Scheduling](https://youtu.be/2fR3KydieKY)

