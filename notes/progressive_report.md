# PChase Progressive Report

## 摘要
实验发现原作者的PChase程序测量结果并不稳定，通过实验提出了一种可将结果稳定的经验性方法。大量使非JIT程序达到PChase性能的尝试都失败了，由于PChase作者使用的API文档丢失，难以进一步分析，但仍然得到了一些可能原因并提出了一些思路。

## PChase 测量结果在XJTU-IAIR服务器上的稳定性研究。
在尝试使非JIT程序达到PChase性能时，偶然发现PChase本身得到的内存带宽在多次实验中也出现了很大差距。随后进行了几次随即参数的PChase执行，提出以下经验性结论：

1. 作者的测试设备规格和本机规格相差太大（年代差距达两个年代），PChase在作者设备上的参数不适合本机测试；
2. 由于本机运算速度可能远远高于作者机器，为保证测试精度应该提高测试的运算量。
3. 考虑到CPU受温度等因素影响频率可能不稳定，采用*重复次数iteration*参数与*链表长度chain_size*参数的乘积作为控制运算量的指标。

设计了以下实验来观察PChase的稳定性：
- 选取了不同链表的队列：8、16、64、256、512（K）
- 每个队列测试两种不同的访问顺序：forward和random
- Iteration基准为50，100，200，300，400，500，600，700，800（G）
- 为了保证同一组测试中不同链表在经过数次运行的总操作量是一样的，链表的实际Iteration会根据链表长度进行修正。
- 实际Iteration计算公式为：$iterations=iterations(Base)/chain_size$

实验结果如下图所示：
![Alt text](PChase%E5%86%85%E5%AD%98%E5%B8%A6%E5%AE%BD%E4%B8%8E%E8%BF%AD%E4%BB%A3%E6%AC%A1%E6%95%B0.png)

得到以下经验性结论：
1. 无论何种链表，其测量得到的内存带宽随着总操作量的增加而先增加后减少，且在总操作量达到一定值后内存带宽基本稳定。
2. 内存带宽的测量值似乎与链表的访问方式无关。
3. PChase如果采用大于等于64k的链表，内存带宽测量值明显降低，同时测量值的稳定性显著增高。

**最重要的是，本实验经验性的指出，当重复次数基准达到600G及以上后，使用各类链表得到的内存带宽都趋于稳定。**

思考：
1. 以后的各类测试可以将重复次数基准取大于600G，以避免测量结果波动。
2. **理论上内存带宽应该和重复次数无关，但实际上重复次数明显影响内存带宽测量值，这是为什么？**

## PChase 非JIT程序性能提升研究
遗憾的是，尽管经过上述实验，PChase的测量结果已经趋于稳定，但是在尝试使非JIT程序达到PChase性能时，仍然没有取得成功。

初版非JIT程序仅仅将PChase中的JIT代码按意思转换为了C++代码，其测试性能很差，遍历相同链表的时间大约是JIT版本的5倍：
```cpp
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
```

分析JIT版本的代码，可以得到一些制约性能的因素。
首先以下面这个JIT函数生成代码中的for循环为例：
```cpp
	for (int i = 0; i < chains_per_thread; i++)
	{
		AsmJit::GPVar head = c.newGP();
		c.mov(head, ptr(chain));
		heads[i] = head;
	}
```
尽管该for循环存在，但是AsmJit不可能在生成函数时以任何方式知道该循环的存在（AsmJit对象只会被相关函数调用），也就是说，**生成的JIT函数中不会存在增加变量i、每次循环时的判断等循环开销**，而是把其中的mov指令重复了若干遍（相当于直接写了chains_per_thread个mov指令）。

基于以上思路，目前的非JIT程序添加了一个init_chase_pointers函数，用于初始化链表头指针，其代码如下：
```cpp
void Run::init_chase_pointers(const Chain **mm, std::vector<const Chain *> &input_head, std:: vector<const Chain *> &input_position)
{
	int64 chain_num = this->exp->chains_per_thread;

	// initialize the heads
	for (int i = 0; i < chain_num; i++)
	{
		input_head[i] = mm[0];
	}

	// mark current position
	for (int i = 0; i < chain_num; i++)
	{
		input_position[i] = input_head[i];
	}
	return ;
}
```
这个函数将所有前置初始化操作都从chase_pointers函数中分离出来，并在每次计时开始前预先执行完毕，实验证明非JIT程序的性能得到了客观的提高，但仍然远远达不到JIT程序的性能。
现在chase_pointers已经被简化如下：
```cpp
void Run::new_chase_pointers(const Chain **mm, std::vector<const Chain *> &input_head, std:: vector<const Chain *> &input_position)
{
	int64 chain_num = this->exp->chains_per_thread;


	// chase pointers
	while (true)
	{
		for (int i = 0; i < chain_num; i++)
		{
			// chase pointer
			input_position[i] = input_position[i]->next;
		}
		// test if end reached
		// all chains are  same length
		// so only need to test one
		if (input_head[0] == input_position[0])
		{
			break;
		}
	}

	return;

}
```
猜想该唯一剩余的for循环影响了性能，于是手动设置chain_num为1，并去除该for循环，性能有一定提升但仍然远远达不到JIT程序的性能。

chase_pointers已难以进一步简化，可以考虑从汇编代码分析两者的差别。**但是PChase使用的API早已废弃，没有文档说明如何导出每次运行时生成的JIT函数汇编代码**。

一种可选的思路：
用新版的AsmJit重构PChase，使其能够得到与旧版相同的测试结果。然后利用新版AsmJit的API导出汇编代码，分析汇编代码中的差别。**（但这也是个大坑，因为新版的AsmJitAPI依然写的很模糊，目前已经尝试了，但是没有成功）**