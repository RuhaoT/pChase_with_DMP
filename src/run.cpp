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

// Include guard
#define PERF_TEST 0
#define DO_BECHMARK 1
#include <sys/syscall.h>

// Implementation header
#include "run.h"

// System includes
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstddef>
#include <algorithm>
#if defined(NUMA)
#include <numa.h>
#endif

// Local includes
// #include <AsmJit/AsmJit.h>
// #include <perf/jitdump.h> //perf depend on asmjit, must first include asmjit
// #include <perf/perfcompiler.h>
#include "timer.h"

//
// Implementation
//

typedef void (*benchmark)(const Chain **);
typedef benchmark (*generator)(int64 chains_per_thread,
							   int64 bytes_per_line, int64 bytes_per_chain,
							   int64 stride, int64 loop_length, int32 prefetch_hint);
/* static benchmark chase_pointers(int64 chains_per_thread,
								int64 bytes_per_line, int64 bytes_per_chain,
								int64 stride, int64 loop_length, int32 prefetch_hint); */

Lock Run::global_mutex;
int64 Run::_ops_per_chain = 0;
std::vector<double> Run::_seconds;

Run::Run() : exp(NULL), bp(NULL)
{
}

Run::~Run()
{
}

void Run::set(Experiment &e, SpinBarrier *sbp)
{
	this->exp = &e;
	this->bp = sbp;
}

int Run::run()
{
	// first allocate all memory for the chains,
	// making sure it is allocated within the
	// intended numa domains
	Chain **chain_memory = new Chain *[this->exp->chains_per_thread];
	Chain **root = new Chain *[this->exp->chains_per_thread];

#if defined(NUMA)
	// establish the node id where this thread
	// will run. threads are mapped to nodes
	// by the set-up code for Experiment.
	int run_node_id = this->exp->thread_domain[this->thread_id()];
	// numa_run_on_node sets the node id where this thread will run
	numa_run_on_node(run_node_id);

	// establish the node id where this thread's
	// memory will be allocated.
	for (int i = 0; i < this->exp->chains_per_thread; i++)
	{
		int alloc_node_id = this->exp->chain_domain[this->thread_id()][i];
		nodemask_t alloc_mask;
		// nonmask_zero sets all bits in the mask to zero
		nodemask_zero(&alloc_mask);
		// nodemask_set sets the bit corresponding to the node id
		nodemask_set(&alloc_mask, alloc_node_id);
		// numa_set_membind sets the memory binding policy for the calling thread
		numa_set_membind(&alloc_mask);

		chain_memory[i] = new Chain[this->exp->links_per_chain];
	}
#else
	for (int i = 0; i < this->exp->chains_per_thread; i++)
	{
		chain_memory[i] = new Chain[this->exp->links_per_chain];
	}
#endif

	// initialize the chains and
	// select the function that
	// will generate the tests
	generator gen;
	for (int i = 0; i < this->exp->chains_per_thread; i++)
	{
		if (this->exp->access_pattern == Experiment::RANDOM)
		{
			root[i] = random_mem_init(chain_memory[i]);
			//gen = chase_pointers;
		}
		else if (this->exp->access_pattern == Experiment::STRIDED)
		{
			if (0 < this->exp->stride)
			{
				root[i] = forward_mem_init(chain_memory[i]);
			}
			else
			{
				root[i] = reverse_mem_init(chain_memory[i]);
			}
			//gen = chase_pointers;
		}
	}

	// compile benchmark
	// bench is a function that will chase pointers
	benchmark bench = gen(this->exp->chains_per_thread,
						  this->exp->bytes_per_line, this->exp->bytes_per_chain,
						  this->exp->stride, this->exp->loop_length,
						  this->exp->prefetch_hint);

	// calculate the number of iterations
	/*
	 * As soon as the thread count rises, this calculation HUGELY
	 * differs between runs. What does cause this? Threads are already
	 * limited to certain CPUs, so it's not caused by excessive switching.
	 * Strange cache behaviour?
	 */

	if (0 == this->exp->iterations)
	{
		volatile static double istart = 0;
		volatile static double istop = 0;
		volatile static double elapsed = 0;
		volatile static int64 iters = 1;
		volatile double bound = std::max(0.2, 10 * Timer::resolution());
		for (iters = 1; elapsed <= bound; iters = iters << 1)
		{
			// barrier
			// ensures all threads start at the same time
			this->bp->barrier();

			// start timer
			if (this->thread_id() == 0)
			{
				istart = Timer::seconds();
			}
			this->bp->barrier();

			// chase pointers
			// NOTE: this is only for measuring time taken per iteration
			// This is NOT the actual benchmark
			for (int i = 0; i < iters; i++)
			{
				if (PERF_TEST)
				{
					sleep(2);
				}
				bench((const Chain **)root);
			}

			// barrier
			// ensures all threads are finished
			this->bp->barrier();

			// stop timer
			if (this->thread_id() == 0)
			{
				istop = Timer::seconds();
				// calculate how long is one iteration
				elapsed = istop - istart;
			}
			this->bp->barrier();
		}

		// calculate the number of iterations
		if (this->thread_id() == 0)
		{
			if (0 < this->exp->seconds)
			{
				this->exp->iterations = std::max(1.0,
												 0.9999 + 0.5 * this->exp->seconds * iters / elapsed);
			}
			else
			{
				this->exp->iterations = std::max(1.0, 0.9999 + iters / elapsed);
			}
			// printf("Tested %d iterations: took %f seconds; scheduling %d iterations\n", iters, elapsed, this->exp->iterations);
		}
		this->bp->barrier();
	}

	// run the experiments
	int num_perf_data = 0;
	for (int e = 0; e < this->exp->experiments; e++)
	{
		// std::cout << "Running experiment " << e << std::endl;
		// barrier
		this->bp->barrier();

		// start timer
		double start = 0;
		if (this->thread_id() == 0)
			start = Timer::seconds();
		this->bp->barrier();

		// chase pointers
		// This is the actual benchmark
		// std::cout << "Iterations: " << this->exp->iterations << std::endl;
		for (int i = 0; i < this->exp->iterations; i++)
		{
			if (PERF_TEST)
			{

				// perf record
				// step 1: get thread tid
				pid_t thread_id = syscall(SYS_gettid);

				// step 2: start perf
				std::string str1 = "eval \"perf record -e --call-graph dwarf -g -t ";
				std::string str2 = std::to_string(thread_id);
				// NOTE:because the use of '-o' option, perf will no longer generate symbol map to /tmp/perf-<pid>.map
				// have to generate the map file manually
				// this should be done in shell script
				std::string str3 = " -o ./perf_";
				std::string str4 = std::to_string(this->thread_id());
				std::string str5 = "_";
				std::string str6 = std::to_string(num_perf_data++);
				std::string str7 = ".data \" & echo $!";
				std::string str = str1 + str2 + str3 + str4 + str5 + str6 + str7;

				pid_t perf_pid = -1;

				// starting perf while obtaining its pid
				std::cout << str.c_str() << std::endl;
				FILE *fp = popen(str.c_str(), "r");
				// std::cout << "Executed perf..." << std::endl;
				if (fp == NULL)
				{
					std::cout << "Failed to execute perf" << std::endl;
					pthread_exit(NULL);
				}
				else
				{
					char buf[1024];
					if (fgets(buf, 1024, fp) != NULL)
					{
						perf_pid = atoi(buf);
					}
					pclose(fp);
					std::cout << "Started perf with pid " << perf_pid << std::endl;
				}
				// prepair the command to stop perf while perf is staring to reduce inaccuracy
				// MUST use kill -INT or else perf.data will be corrupted
				str1 = "kill -INT ";
				str2 = std::to_string(perf_pid);
				std::string command = str1 + str2;

				// step 3: run benchmark
				// sleep enough time for perf to start
				if (DO_BECHMARK)
				{
					sleep(2);
					bench((const Chain **)root);
				}
				else
				{
					sleep(2);
				}

				// step 4: stop perf
				system(command.c_str());
				std::cout << "Stopped perf with pid " << str2 << std::endl;
			}
			else
			{
				new_chase_pointers((const Chain **)root);
			}
		}
		// barrier
		this->bp->barrier();

		// stop timer
		double stop = 0;
		if (this->thread_id() == 0)
			stop = Timer::seconds();
		this->bp->barrier();

		if (0 <= e)
		{
			if (this->thread_id() == 0)
			{
				double delta = stop - start;
				if (0 < delta)
				{
					Run::_seconds.push_back(delta);
				}
			}
		}

	}

	this->bp->barrier();

	// clean the memory
	for (int i = 0; i < this->exp->chains_per_thread; i++)
	{
		if (chain_memory[i] != NULL)
			delete[] chain_memory[i];
	}
	if (chain_memory != NULL)
		delete[] chain_memory;

	return 0;
}

// dummy is used to prevent the compiler from optimizing out the function?
int dummy = 0;
void Run::mem_check(Chain *m)
{
	if (m == NULL)
		dummy += 1;
}

// exclude 2 and Mersenne primes, i.e.,
// primes of the form 2**n - 1, e.g.,
// 3, 7, 31, 127
static const int prime_table[] = {
	5,
	11,
	13,
	17,
	19,
	23,
	37,
	41,
	43,
	47,
	53,
	61,
	71,
	73,
	79,
	83,
	89,
	97,
	101,
	103,
	109,
	113,
	131,
	137,
	139,
	149,
	151,
	157,
	163,
};
static const int prime_table_size = sizeof prime_table / sizeof prime_table[0];

Chain *
Run::random_mem_init(Chain *mem)
{
	// initialize pointers --
	// choose a page at random, then use
	// one pointer from each cache line
	// within the page.  all pages and
	// cache lines are chosen at random.
	Chain *root = 0;
	Chain *prev = 0;
	int link_within_line = 0;
	int64 local_ops_per_chain = 0;

	// we must set a lock because random()
	// is not thread safe
	// the reason to choose a page at prime number intervals is to avoid cache conflicts?
	Run::global_mutex.lock();
	setstate(this->exp->random_state[this->thread_id()]);
	int page_factor = prime_table[random() % prime_table_size];
	int page_offset = random() % this->exp->pages_per_chain;
	Run::global_mutex.unlock();

	// loop through the pages
	for (int i = 0; i < this->exp->pages_per_chain; i++)
	{
		int page = (page_factor * i + page_offset) % this->exp->pages_per_chain;
		Run::global_mutex.lock();
		setstate(this->exp->random_state[this->thread_id()]);
		int line_factor = prime_table[random() % prime_table_size];
		int line_offset = random() % this->exp->lines_per_page;
		Run::global_mutex.unlock();

		// loop through the lines within a page
		for (int j = 0; j < this->exp->lines_per_page; j++)
		{
			int line_within_page = (line_factor * j + line_offset) % this->exp->lines_per_page;
			int link = page * this->exp->links_per_page + line_within_page * this->exp->links_per_line + link_within_line;

			if (root == 0)
			{
				prev = root = mem + link;
				local_ops_per_chain += 1;
			}
			else
			{
				prev->next = mem + link;
				prev = prev->next;
				local_ops_per_chain += 1;
			}
		}
	}

	prev->next = root;

	Run::global_mutex.lock();
	Run::_ops_per_chain = local_ops_per_chain;
	Run::global_mutex.unlock();

	return root;
}

Chain *
Run::forward_mem_init(Chain *mem)
{
	Chain *root = 0;
	Chain *prev = 0;
	// link_within_line has no effect
	int link_within_line = 0;
	int64 local_ops_per_chain = 0;

	for (int i = 0; i < this->exp->lines_per_chain; i += this->exp->stride)
	{
		int link = i * this->exp->links_per_line + link_within_line;
		if (root == NULL)
		{
			prev = root = mem + link;
			local_ops_per_chain += 1;
		}
		else
		{
			prev->next = mem + link;
			prev = prev->next;
			local_ops_per_chain += 1;
		}
	}

	prev->next = root;

	Run::global_mutex.lock();
	Run::_ops_per_chain = local_ops_per_chain;
	Run::global_mutex.unlock();

	return root;
}

Chain *
Run::reverse_mem_init(Chain *mem)
{
	Chain *root = 0;
	Chain *prev = 0;
	int link_within_line = 0;
	int64 local_ops_per_chain = 0;

	int stride = -this->exp->stride;
	int last;
	for (int i = 0; i < this->exp->lines_per_chain; i += stride)
	{
		last = i;
	}

	for (int i = last; 0 <= i; i -= stride)
	{
		int link = i * this->exp->links_per_line + link_within_line;
		if (root == 0)
		{
			prev = root = mem + link;
			local_ops_per_chain += 1;
		}
		else
		{
			prev->next = mem + link;
			prev = prev->next;
			local_ops_per_chain += 1;
		}
	}

	// the final link points to the first link
	// this is used to determind when the chain is finished
	prev->next = root;

	Run::global_mutex.lock();
	Run::_ops_per_chain = local_ops_per_chain;
	Run::global_mutex.unlock();

	return root;
}

void Run::new_chase_pointers(const Chain **mm)
{
	std::vector<Chain *> heads(this->exp->chains_per_thread);

	// initialize the heads
	for (int i = 0; i < this->exp->chains_per_thread; i++)
	{
		heads[i] = (Chain *)mm[i];
	}

	// mark current position
	std::vector<Chain *> positions(this->exp->chains_per_thread);
	for (int i = 0; i < this->exp->chains_per_thread; i++)
	{
		positions[i] = heads[i];
	}

	// chase pointers
	while (true)
	{
		for (int i = 0; i < this->exp->chains_per_thread; i++)
		{
			positions[i] = positions[i]->next;
		}

		// test if end reached
		// all chains are  same length
		// so only need to test one
		if (heads[0] == positions[0])
		{
			break;
		}
	}

	return;

}

// decrapated in this version
/* static benchmark chase_pointers(int64 chains_per_thread, // memory loading per thread
								int64 bytes_per_line,	 // ignored
								int64 bytes_per_chain,	 // ignored
								int64 stride,			 // ignored
								int64 loop_length,		 // length of the inner loop
								int32 prefetch_hint		 // use of prefetching
)
{
	// NOTE: because of the poor support of perf to profile JIT code, the code is modified to include jitdump
	// the jitdump class will create symbol map file for perf to use
	// Create Compiler.
	// use PerfCompiler instead of Compiler
	AsmJit::Compiler c;

	// Tell compiler the function prototype we want. It allocates variables representing
	// function arguments that can be accessed through Compiler or Function instance.
	c.newFunction(AsmJit::CALL_CONV_DEFAULT, AsmJit::FunctionBuilder1<AsmJit::Void, const Chain **>());
	// refactor line above with lastest version of asmjit

	// Try to generate function without prolog/epilog code:
	c.getFunction()->setHint(AsmJit::FUNCTION_HINT_NAKED, true);

	// Create labels.
	AsmJit::Label L_Loop = c.newLabel();

	// Function arguments.
	AsmJit::GPVar chain(c.argGP(0));

	// Save the head
	std::vector<AsmJit::GPVar> heads(chains_per_thread);
	for (int i = 0; i < chains_per_thread; i++)
	{
		AsmJit::GPVar head = c.newGP();
		c.mov(head, ptr(chain));
		heads[i] = head;
	}

	// Current position
	std::vector<AsmJit::GPVar> positions(chains_per_thread);
	for (int i = 0; i < chains_per_thread; i++)
	{
		AsmJit::GPVar position = c.newGP();
		c.mov(position, heads[0]);
		positions[i] = position;
	}

	// Loop.
	c.bind(L_Loop);

	// Process all links
	for (int i = 0; i < chains_per_thread; i++)
	{
		// Chase pointer
		c.mov(positions[i], ptr(positions[i], offsetof(Chain, next)));

		// Prefetch next
		switch (prefetch_hint)
		{
		case Experiment::T0:
			c.prefetch(ptr(positions[i]), AsmJit::PREFETCH_T0);
			break;
		case Experiment::T1:
			c.prefetch(ptr(positions[i]), AsmJit::PREFETCH_T1);
			break;
		case Experiment::T2:
			c.prefetch(ptr(positions[i]), AsmJit::PREFETCH_T2);
			break;
		case Experiment::NTA:
			c.prefetch(ptr(positions[i]), AsmJit::PREFETCH_NTA);
			break;
		case Experiment::NONE:
		default:
			break;
		}
	}

	// Wait
	// wait for the prefetches to complete
	for (int i = 0; i < loop_length; i++)
		c.nop();

	// Test if end reached
	// equal means that the chain is finished
	c.cmp(heads[0], positions[0]);
	c.jne(L_Loop);

	// Finish.
	c.endFunction();

	// Make JIT function.
	benchmark fn = AsmJit::function_cast<benchmark>(c.make());

	// Ensure that everything is ok.
	if (!fn)
	{
		printf("Error making jit function (%u).\n", c.getError());
		return 0;
	}

	return fn;
}
 */