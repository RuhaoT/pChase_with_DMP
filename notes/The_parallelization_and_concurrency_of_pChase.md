# The Parallelization and Concurrency of pChase

## 摘要
本文总结了pChase中用于测试访存性能的线程是如何实现并行与并发的，并指出了一些pChase的可能改进之处。
pChase对线程的并行执行是“尽力而为”的，测试时不同CPU间的线程并行执行，同一CPU间无法并行的线程并发执行，CPU间负载均衡。
为了实现以上顶层思想，pChase①在线程创建时尽可能均匀的将线程分给CPU，②通过barrier控制测试时线程统一执行。
出于未知原因，pChase支持同时调用的最大CPU个数为8个，本文认为通过修改pChase参数可以充分利用CPU更多的系统。

## 1. 线程创建时的并行性与并发性考量
在线程创建时，pChase的基本思想是将线程依次分配给每一个可用的CPU，直到所有线程都被分配完毕。这样当线程数小于CPU数量时，每个线程都有一个独立的CPU可供执行，而当线程数多于CPU个数时，CPU间的负载（线程个数）是尽量均衡的。

以上过程代码的自然语言描述为：
0. 线程被创建并开始运行。
1. 初始化一个CPU集合`cpu_set_t`，这是一个位向量，每一位记录着一个CPU有没有被分配到。
2. 检查当前线程可以在哪些CPU上运行，将这些CPU的位设置为1。
3. 统计`cpu_set_t`中1的个数，即当前线程可以运行的CPU个数，记为`count`。
4. 每个线程都有一个独有的`id`，`id`的大小即线程是第几个被创建的，通过`id % count`得到现在应该把线程分发给哪个CPU，并把`cpu_set_t`中对应的位设置为1，其余位置为0。
5. 通过`sched_setaffinity`和`cpu_set_t`将线程绑定到对应的CPU上。
6. 结束，线程继续在当前CPU上执行，**此时不同CPU上的线程已经开始并行执行了**。

我在源代码上添加了大量注释，具体代码请见[这里](https://github.com/RuhaoT/pChase/blob/master/src/thread.cpp)。

## 2. 测试时的并行性与并发性考量
在线程创建完成后，会经历存储空间初始化等一系列操作，等程序运行到每个线程要作为测试线程开始pointer chasing时，这些线程的执行时间可能差别很大。由此，pChase使用了barrier函数来控制所有线程在同一时刻开始执行，这样就可以保证所有线程在同一时刻开始，不会影响到测试。

barrier函数的使用代码如下：
```c++
		// barrier
		this->bp->barrier();
```

具体请见[这里](https://github.com/RuhaoT/pChase/blob/master/src/run.cpp)。

## 3. 可能的改进
观察，线程创建时的代码，以下是用于统计可用CPU个数的代码：
```c++
	int count = 0;
	for (int i = 0; i < 8; i++)
	{
		// CPU_ISSET function tests whether the bit corresponding to cpu is set in the CPU set set.
		if (CPU_ISSET(i, &cs))
				count++;
	}
```
可见不知为何，当可用CPU个数大于8个后，其余CPU就不在可选范围之内了，建议把这个数字改为一个用户变量，这样就可以充分利用CPU更多的系统了。