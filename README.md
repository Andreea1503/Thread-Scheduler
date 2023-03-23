### HOMEWORK 4 - THREADS SCHEDULER ###

## STUCTURE ##

The implementation of the homework uses two stuctures and an enum:
- ``` so_thread```, which contains the characteristics of every thread on
execution (events, priority, remaining time, etc);

- ``` so_scheduler```, which contains the characteristics of the scheduler
(threads, current thread, time quantum, number of threads, etc);

- ```thread_status```, which contains the possible states of a thread.
(READY, RUNNING, WAITING, etc).

## HOMEWORK IMPLEMENTATION ##

The homework consists of implementing a scheduler that can run multiple
threads in parallel. The scheduler simulates the execution of threads
on a CPU, which can only execute one thread at a time. The scheduler
must be able to switch between threads, and to execute them in a
round-robin style. The scheduler must also be able to handle the
following events: thread creation, thread termination, thread
execution, thread waiting, thread signaling, thread blocking, thread
unblocking, thread priority change, thread time quantum change.

## IMPLEMENTATION DETAILS ##

Every thread is added to the priority queue of the scheduler, then blocked
with the help of a mutex(lock, realised with the help of so_fork function).
Then, a function which decides if the thread should run in the current
time quantum or not is called. For this thread, the mutex of the thread that
was previously blocked, is unlocked.

When a thread is terminated, the ```reschedule``` function is called, which
unblocks the next thread in the priority queue, the one with the highest
priority. It is executed similary, then readded to the queue of the finished
threads.

In the end, the threads are joined, and the scheduler is destroyed.

## REFERENCES ##

- [1] old labs 8 & 9
- [2] the old courses
- [3] the header files