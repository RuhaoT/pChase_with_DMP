# The prefetch mechanism of pChase

## 摘要
本文总结了pChase原版中提供的指令预取方法。pChase使用了旧版本AsmJit中包含的X86预API，在转换为JIT函数后，这些API向X86处理器传递Streaming SIMD Extension (SSE) 指令集预取命令。

## pChase中的AsmJit指令预取API
**注意：由于pChase使用的AsmJit版本过低，其相关API早已被官方文档移除，本文对API的分析包含一定推断成分**

pChase源码`src/run.cpp`中使用了以下API：
```cpp
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
```

其中c为`Compiler`类型，`prefetch`方法的定义在`lib/AsmJit/CompilerX86X64.h`
```cpp
  //! @brief Prefetch (SSE).
  inline void prefetch(const Mem& mem, const Imm& hint)
  {
    _emitInstruction(INST_PREFETCH, &mem, &hint);
  }
```

根据注释和[AsmJit官网对新版API的介绍](https://asmjit.com/doc/namespaceasmjit_1_1x86_1_1Inst.html#a69d659d9299b33041d906287aa1080c9a0f7ebd614e2855aedfb7fa0ad766dd2c)可知此方法用于向X86处理器传递SSE指令集的预取命令。pChase中提供的`PREFETCH_HINT`每个（除了`NONE`外）分别对应一条不同的SSE预取指令。

## SSE指令集中的预取命令
从[GNU对SSE指令集的介绍](https://gcc.gnu.org/projects/prefetch.html#ia32_sse)可见SSE指令集中的预取指令包含以下几种：

|指令|介绍|
|-|-|
|prefetcht0|	Temporal data; prefetch data into all cache levels.|
|prefetcht1|	Temporal with respect to first level cache; prefetch data in all cache levels except 0th cache level.|
|prefetcht2|	Temporal with respect to second level cache; prefetch data in all cache levels, except 0th and 1st cache levels.|
|prefetchnta|	Non-temporal with respect to all cache levels; prefetch data into non-temporal cache structure, with minimal cache pollution.|