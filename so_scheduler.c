#include "so_scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include "helper.h"

int so_init(unsigned int time_quantum, unsigned int io)
{
	/*
	 * Verifying if the scheduler is already initialized
	 */
	ASSERT(!sched, return ERROR;);
	ASSERT(time_quantum > 0, return ERROR;);
	ASSERT(io <= SO_MAX_NUM_EVENTS, return ERROR;);

	/*
	 * Allocate memory for the scheduler
	 */
	sched = malloc(sizeof(scheduler));
	ASSERT(sched, return ERROR;);

	/*
	 * Initialize scheduler's fields
	 */
	sched->max_events = io;
	sched->queue_size = 0;
	sched->num_threads = 0;
	sched->current_thread = NULL;
	sched->time_quantum = time_quantum;

	/*
	 * Allocate memory for the threads array
	 */
	sched->threads = malloc(NUM_THREADS * sizeof(so_thread *));
	ASSERT(sched->threads, return ERROR;);

	/*
	 * Allocate memory for the priority queue
	 */
	sched->ready_queue = malloc(NUM_THREADS * sizeof(so_thread *));
	ASSERT(sched->ready_queue, return ERROR;);

	return 0;
}

/*
 * function that schedules the next thread to run on the processor
 */
int reschedule(void)
{
	so_thread *current_thread = sched->current_thread;

	/*
	 * If there are no threads on the processor, I unblock the current thread
	 * so it can enter the processor and run its task
	 */
	if (!get_next_thread()) {
		sem_post(&current_thread->permission);

		return READY;
	/*
	 * If the current thread is in the WAITING or TERMINATED state,
	 * the next thread will enter the processor to run its task
	 */
	} else {
		so_thread *next_thread = get_next_thread();

		if (!current_thread ||
			current_thread->status == WAITING ||
			current_thread->status == TERMINATED) {
			sched->current_thread = next_thread;
			start_thread();

			return READY;

		/*
		 * The next thread is chosen according to the Round Robin algorithm
		 */
		} else if (Round_Robin(current_thread, next_thread))
			return READY;
	}

	/*
	 * If it reached this point, it means that the current thread
	 * will continue to run on the processor
	 */
	return 0;
}

/*
 * function called by every thread after the pthread_create function
 * is called
 */
void *thread_func(void *arg)
{
	so_thread *current_thread = (so_thread *)arg;

	/*
	 * the thread waits for the permission to enter the processor
	 */
	sem_wait(&current_thread->permission);

	/*
	 * the handler is called
	 */
	current_thread->handler(current_thread->priority);

	/*
	 * the thread is terminated
	 */
	current_thread->status = TERMINATED;

	/*
	 * the schedule is updated
	 */
	if (!reschedule())
		sem_post(&current_thread->permission);

	return NULL;
}

/*
 * The function initializes a thread
 */
void initialize_thread(so_thread *thread, so_handler *func,
						unsigned int priority)
{
	/*
	 * the fields of the thread are initialized
	 */
	thread->priority = priority;
	thread->remaining_time = sched->time_quantum;
	thread->handler = func;
	thread->tid = INVALID_TID;
	thread->status = NEW;
	thread->events = SO_MAX_NUM_EVENTS;

	/*
	 * the semaphore is initialized
	 */
	sem_init(&thread->permission, 0, 0);
}


tid_t so_fork(so_handler *func, unsigned int priority)
{
	unsigned int pos;

	so_thread *current_thread;

	/*
	 * Verifying the validity of the arguments
	 */
	ASSERT(func, return INVALID_TID;);

	if (priority > SO_MAX_PRIO)
		return INVALID_TID;

	/*
	 * Creating a new thread, which will execute the start function
	 */
	current_thread = malloc(sizeof(so_thread));
	ASSERT(current_thread, return INVALID_TID;);

	initialize_thread(current_thread, func, priority);

	/*
	 * the thread is sent to the processor
	 */
	pthread_create(&current_thread->tid, NULL,
						&thread_func, (void *) current_thread);

	/*
	 * the thread is added to the threads array
	 * and the number of threads is incremented
	 */
	sched->threads[sched->num_threads++] = current_thread;

	/*
	 * adding the thread to the priority queue
	 */
	current_thread->status = READY;
	pos = get_position(current_thread);
	shift_queue(pos);
	++sched->queue_size;
	sched->ready_queue[pos] = current_thread;

	/*
	 * if there is a thread running on the processor,
	 * it comsumes the time on the processor
	 * then the next thread is scheduled
	 */
	if (sched->current_thread)
		so_exec();

	/*
	 * otherwise the next thread is chosen
	 */
	else if (!reschedule())
		sem_post(&current_thread->permission);

	/*
	 * return the id of the new thread
	 */
	return current_thread->tid;
}

int so_wait(unsigned int io)
{
	/*
	 * if the argument is valid, the current thread is blocked
	 */
	if (io < sched->max_events) {
		/*
		 * the current thread is blocked, waiting for the i/o event
		 */
		sched->current_thread->status = WAITING;
		sched->current_thread->events = io;

		/*
		 * the thread consumes the time on the processor,
		 * then the next thread is scheduled
		 */
		so_exec();

		return 0;
	}

	/*
	 * if the argument isn't valid, return an error
	 */
	return ERROR;
}

int so_signal(unsigned int io)
{
	int unlocked_threads = 0;
	/*
	 * validating the argument io
	 */
	ASSERT(io < sched->max_events, return ERROR;);

	/*
	 * every thread that is waiting for the i/o event
	 * is unblocked and added to the priority queue
	 * and the number of threads in the ready queue is incremented
	 */
	for (int i = 0; i < sched->num_threads; ++i) {
		so_thread *it = sched->threads[i];

		if (it->events == io && it->status == WAITING) {
			it->status = READY;
			int pos = get_position(it);

			shift_queue(pos);
			++sched->queue_size;
			sched->ready_queue[pos] = it;

			/*
			 * number of threads that are unblocked is incremented
			 */
			++unlocked_threads;
		}
	}

	/*
	 * the current thread consumes the time on the processor,
	 * then the next thread is scheduled
	 */
	so_exec();

	/*
	 * return the number of threads that were unblocked
	 */
	return unlocked_threads;
}

void so_exec(void)
{
	so_thread *current_thread = sched->current_thread;

	/*
	 * marking the current thread as running on the processor,
	 * by decrementing the quantum time for the current thread
	 */
	--current_thread->remaining_time;

	/*
	 * Scheduling the next thread that will run on the processor
	 * if the currrent thread will run on the processor again
	 * the semaphore is unlocked, so the process can continue
	 * otherwise the current thread is preempted and its
	 * semaphore is locked until it runs on the processor again
	 */
	if (!reschedule())
		sem_post(&current_thread->permission);

	sem_wait(&current_thread->permission);
}

void so_end(void)
{
	u_int16_t i;

	/*
	 * if the scheduler is NULL, there is no need to free the memory
	 */
	ASSERT(sched, return;);

	/*
	 * the scheduler waits for all the threads to finish
	 * if the thread has finished, the semaphore is freed
	 */
	i = 0;
	while (i < sched->num_threads) {
		pthread_join(sched->threads[i]->tid, NULL);
		sem_destroy(&sched->threads[i]->permission);
		free(sched->threads[i]);
		++i;
	}

	/*
	 * freeing the memory allocated for the threads and
	 * the scheduler
	 */
	free(sched->threads);
	free(sched->ready_queue);
	free(sched);

	if (sched)
		sched = NULL;
}
