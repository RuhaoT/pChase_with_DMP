/*******************************************************************************
 * Copyright (c) 2006 International Business Machines Corporation.             *
 * All rights reserved. This program and the accompanying materials            *
 * are made available under the terms of the Common Public License v1.0        *
 * which accompanies this distribution, and is available at                    *
 * http://www.opensource.org/licenses/cpl1.0.php                               *
 *                                                                             *
 * Contributors:                                                               *
 *    Douglas M. Pase - initial API and implementation                         *
 *    Tim Besard - prefetching, JIT compilation                                *
 *******************************************************************************/

//
// Configuration
//

// Implementation header
#include "thread.h"

// System includes
#include <cstdio>
#include <pthread.h>
#include <unistd.h>

Lock Thread::_global_lock;
int Thread::count = 0;

//
// Implementation
//

Thread::Thread()
{
	Thread::global_lock();
	// count is a static variable, so each time a new thread is created, count will be incremented by 1
	// also each Thread has a unique id
	this->id = Thread::count;
	Thread::count += 1;
	Thread::global_unlock();
}

Thread::~Thread()
{
}

// thread will start at function start_routine
int Thread::start()
{
	return pthread_create(&this->thread, NULL, Thread::start_routine, this);
}

void *
Thread::start_routine(void *p)
{
	// get the current affinity
	// type cpu_set_t is a bitset where each bit represents a CPU
	cpu_set_t cs;
	// CPU_ZERO function initializes the CPU set set to be the empty set.
	CPU_ZERO(&cs);
	// with pid=0, sched_getaffinity masks current thread to the CPU_set_t structure
	sched_getaffinity(0, sizeof(cs), &cs);

	// count how many CPUs on which the process is allowed to run
	int count = 0;
	for (int i = 0; i < 8; i++)
	{
		// CPU_ISSET function tests whether the bit corresponding to cpu is set in the CPU set set.
		if (CPU_ISSET(i, &cs))
			count++;
	}

	/* Branch cpu_affinity_test  */
	int cpu_affinity_test = 0;
	if (cpu_affinity_test)
	{
		// get real system cpu number
		int cpu_num = sysconf(_SC_NPROCESSORS_CONF);

		// test how many CPUs on which the process is allowed to run
		int real_count = 0;
		for (int i = 0; i < cpu_num; i++)
		{
			if (CPU_ISSET(i, &cs))
				real_count++;
		}

		// print out the result
		printf("Thread %d: %d CPUs allowed, %d CPUs available\n", ((Thread *)p)->id, count, real_count);
		fflush(stdout);

		
		CPU_ZERO(&cs);
		
		// check if thread could be moved to ideal CPU
		size_t size = CPU_ALLOC_SIZE(1);
		CPU_SET_S(((Thread *)p)->id % real_count, size, &cs);
		sched_setaffinity(pthread_self(), size, &cs);
		// no error here means affinity set successfully
		
		// exit because this branch is only for testing
		pthread_exit(NULL);
	}

	// restrict to a single CPU
	// ensures that the thread is not moved to another CPU
	CPU_ZERO(&cs);
	// CPU_ALLOC_SIZE takes the number of CPUs as an argument and returns the number of bytes required to hold a CPU set containing that many CPUs.
	size_t size = CPU_ALLOC_SIZE(1);
	// CPU_SET_S function takes the CPU number cpu as an argument and sets the corresponding bit in the CPU set set.
	// this creates a CPU set with only one CPU is set, which is the id of the thread mod count
	CPU_SET_S(((Thread *)p)->id % count, size, &cs);
	// pthread_self() returns the ID of the calling thread, and sched_setaffinity set its affinity to the selected CPU
	sched_setaffinity(pthread_self(), size, &cs);
	// assume 8 is the maximum number of CPUs, then this method will make CPUs take turns to run the threads

	// run
	// the thread will then execute the run() method
	// however the run() method is not defined in the Thread class
	// the run() method is defined in the subclass of Thread--run
	((Thread *)p)->run();

	return NULL;
}

void Thread::exit()
{
	pthread_exit(NULL);
}

int Thread::wait()
{
	pthread_join(this->thread, NULL);

	return 0;
}

void Thread::lock()
{
	this->object_lock.lock();
}

void Thread::unlock()
{
	this->object_lock.unlock();
}

void Thread::global_lock()
{
	Thread::_global_lock.lock();
}

void Thread::global_unlock()
{
	Thread::_global_lock.unlock();
}
