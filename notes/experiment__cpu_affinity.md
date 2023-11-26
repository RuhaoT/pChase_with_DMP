# Experiment: CPU affinity

## 摘要
在“The_parallelization_and_concurrency_of_pChase.md”中提出，pChase最多只能利用8个cpu，我认为这是可以改进的。通过实验证明了这一点——适当的修改就能利用系统中所有可用的CPU。

## 1. 实验准备
在[thread.cpp]()中添加了如下代码：
```cpp
	int cpu_affinity_test = 1;
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
```
这段代码的作用是：首先获取系统中的CPU数量，然后检查当前线程可以运行在哪些CPU上，最后将当前线程移动到一个理想的CPU上（而不仅限于pChase原来提供的8个cpu）。这段代码的最后一句是`pthread_exit(NULL)`，这是因为这段代码只是用来测试的，不需要执行其他的操作。

## 2. 实验结果
实验分别测试了线程数为1、2、4、8、16、32、64、128、256时的情况，实验程序将线程分配到了全部48个可用CPU中。没有任何报错。

## 3. 结论
pChase可以利用系统中的所有CPU，而不仅仅是8个。只需要在分配时获取系统中的CPU数量，然后将线程分配到这些CPU中即可。同时，还可以人为指定可用CPU的数量，这样可以更好地控制线程的运行。