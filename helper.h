#ifndef HELPER_H_
#define HELPER_H_

#include <pthread.h>
#include <semaphore.h>

#define ASSERT(test, err_code) do {\
		if (!(test)) {\
			err_code\
	} \
} while (0)

/*
 * the states of the thread
 * the error state, returned by the ERROR macro
 */
#define ERROR -1

typedef enum {
	NEW,
	READY,
	RUNNING,
	WAITING,
	TERMINATED
} thread_status;

/*
 * maximum number of threads
 */
#define NUM_THREADS 1000

/*
 * the thread structure
 */
typedef struct {
	tid_t tid;
	u_int16_t events;
	u_int16_t priority;
	u_int32_t remaining_time;
	thread_status status;
	so_handler *handler;
	sem_t permission;
} so_thread;

/*
 * the scheduler structure
 */
typedef struct {
	u_int16_t max_events;
	u_int16_t queue_size;
	u_int32_t num_threads;
	u_int16_t time_quantum;

	so_thread *current_thread;
	so_thread **threads;
	so_thread **ready_queue;
} scheduler;



/*
 * defines the scheduler
 */
static scheduler *sched;


/*
 * shifts the priority queue to the left, starting from the position pos,
 * to make room for a new thread, which will be added
 */
static void shift_queue(unsigned int pos)
{
	for (u_int16_t j = sched->queue_size; j > pos; --j)
		sched->ready_queue[j] = sched->ready_queue[j - 1];
}

/*
 * returns the position where the thread should be inserted
 */
static unsigned int get_position(so_thread *thread)
{
	u_int16_t i = 0;

	/*
	 * searching for the position of the new thread:
	 * after the first thread with a lower priority
	 */

    for (i = 0; i < sched->queue_size && (sched->ready_queue[i]->priority < thread->priority); ++i)
		;

	return i;
}

/*
 * function that returns the new thread, the one with a higher priority
 */
so_thread *get_next_thread(void)
{

	/*
	 * if there aren't any threads in the queue, the function returns NULL
	 */
	ASSERT(sched->queue_size != 0, return NULL;);

	/*
	 * the thread with the highest priority is in the last position
	 * in the ready queue, if there are elements in the queue
	 */

	if (sched->queue_size)
		return sched->ready_queue[sched->queue_size - 1];

	return NULL;
}

static void start_thread(void)
{
	/*
	 * the thread is removed from the ready queue,
	 * and the status is changed to RUNNING, from READY
	 */
	sched->ready_queue[--sched->queue_size] = NULL;
	sched->current_thread->status = RUNNING;

	/*
	 * the semaphore of the chosen thread is freed,
	 * so that it can start running on the processor
	 */
	sem_post(&sched->current_thread->permission);
}

/*
 * Switches between threads
 */
static u_int8_t switch_threads(so_thread *current, so_thread *next)
{
		/*
		 * the current thread is preempted
		 */
		current->status = READY;
		current->remaining_time = sched->time_quantum;

		unsigned int pos = get_position(current);

		shift_queue(pos);

		++sched->queue_size;
		sched->ready_queue[pos] = current;

		/*
		 * the thread with the highest priority is chosen
		 */
		sched->current_thread = next;
		start_thread();

		return 1;
}

/*
 * function that implements the Round Robin algorithm
 */
static int Round_Robin(so_thread *current, so_thread *next)
{
	u_int8_t rc = 0;
	/*
	 * if there's any thread with a higer priority or
	 * the current thread's time quantum has expired, and
	 * there's another thread with the same priority,
	 * thread switching is performed
	 */
	if (current->priority < next->priority) {
		rc = switch_threads(current, next);
		return rc;
	}

	if ((current->remaining_time <= 0 &&
		current->priority == next->priority)) {
		rc = switch_threads(current, next);
		return rc;
	}

	/*
	 * is the current thread's time quantum has expired,
	 * and there are no other threads with a higher priority,
	 * the thread will continue to run on the processor
	 * and its time quantum will be reset
	 */
	if (current->remaining_time <= 0)
		current->remaining_time = sched->time_quantum;

	return rc;
}

#endif /* HELPER_H_ */
