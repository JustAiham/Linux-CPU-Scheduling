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

### Kernel Version

```bash
uname -r
```

### Scheduler Source Code

The scheduler implementation is located in `kernel/sched/`.

#### Important Files

- **core.c:** Core scheduling logic, including `__schedule()` and `context_switch()`.
- **fair.c:** Fair scheduling implementation.
- **rt.c:** Real-time scheduling implementation.
- **deadline.c:** Deadline scheduling implementation.

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