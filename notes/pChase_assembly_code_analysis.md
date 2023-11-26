# PChase Assembly Code Analysis

## 摘要
本实验分为两个部分，分别尝试对AArch64架构和X86架构下的pChase程序进行汇编代码分析，从理论上验证其可以被DMP预取器成功预取。对AArch64架构的编译以失败告终，并得到pChase无法在不进行大量修改的情况下在AArch64架构上运行的结论。对x86架构的代码分析得到了DMP几乎不可能在当前使用AsmJit生成的代码中成功预取的结论，因为大量来自AsmJit处理指令的访存淹没了DMP预取器的访存观察窗口，同时由于所使用的AsmJit版本已经不受支持，许多细节分析无法进行。

## PChase在AArch64架构上的汇编代码分析
实验采用了ARM GNU Toolchain进行编译。所选用的Toolchain具体为：
```
arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu
```
编译未能成功，核心错误语句如下：
```
/home/stu1/Work/ruhao_tian/pChase/lib/AsmJit/AssemblerX86X64.h: In member function ‘sysuint_t AsmJit::AssemblerCore::relocCode(void*) const’:
/home/stu1/Work/ruhao_tian/pChase/lib/AsmJit/AssemblerX86X64.h:542:27: error: cast from ‘void*’ to ‘sysuint_t’ {aka ‘unsigned int’} loses precision [-fpermissive]
  542 |     return relocCode(dst, (sysuint_t)dst);
      |         
```
编译中对AsmJit库中的多个函数均出现了相似的类型转换错误。
**经过资料查找，确定错误原因为AArch64架构与pChase所使用的AsmJit版本不相互兼容**
**在pChase所使用的AsmJit源码中，大量使用了指针向整形的转换，这在X86架构中受到支持，但在AArch64中指针大小与整形大小不匹配，如下表：**
| 架构 | 指针大小 | 整形大小 |
| ---- | -------- | -------- |
| X86  | 32位     | 32位     |
| AArch64 | 64位  | 32位     |
因此，pChase无法在AArch64架构中运行，以下是两种可行的修改方案：
1. 由于本项目中已明确不需要JIT功能，可使用C++标准库重写JIT部分。
2. 使用兼容AArch64的高版本AsmJit库重构相关部分。

## PChase在X86架构上的汇编代码分析
对X86架构下汇编代码的直接分析无法进行，这是因为AsmJit对`mov`等常规机器指令在以JIT形式调用时进行了大量封装和处理，因此转而先分析C语言中AsmJit的工作流程。
C语言中PChase的指针追逐核心代码如下：
```c
	c.mov(positions[i], ptr(positions[i], offsetof(Chain, next)));
```
AsmJit并未将该代码直接转化成对应的机器码，而是执行了大量处理。
1. AsmJit首先将指令和操作数添加到了编译器要生成的队列中：
```c
  inline void mov(const GPVar& dst, const Mem& src)
  {
    _emitInstruction(INST_MOV, &dst, &src);
  }
```
其中`emitInstrucion`的函数定义如下：
```c
void CompilerCore::_emitInstruction(uint32_t code, const Operand* o0, const Operand* o1) ASMJIT_NOTHROW
{
  Operand* operands = reinterpret_cast<Operand*>(_zone.zalloc(2 * sizeof(Operand)));
  if (!operands) return;

  operands[0] = *o0;
  operands[1] = *o1;

  EInstruction* e = newInstruction(code, operands, 2);
  if (!e) return;

  addEmittable(e);
  if (_cc) { e->_offset = _cc->_currentOffset; e->prepare(*_cc); }
}
```
2. AsmJit生成了新指令对象e(由于该版本早已不受支持，无法找到生成的相关细节)
3. AsmJit将新指令对象e添加到了编译器的队列中，即`addEmittable(e)`。
4. 编译器执行。

经过对汇编代码的分析，确认以上所有对应函数的机器码版本均得到执行。

**以上AsmJit的具体工作原理并非重点，而是其中进行的极大量访存操作淹没了DMP预取器可以生成differential match的最大窗口（64次cache访存）**

下面从汇编语言的角度具体分析AsmJit的工作流程，并记录其中的访存次数。
1. PChase中指针追逐的主循环，该循环单次中的总访存次数决定了DMP能否在下一次执行循环时找到访存流（stream）
```asm
1589b:	8b 85 70 fe ff ff    	mov    -0x190(%rbp),%eax
   158a1:	48 63 d0             	movslq %eax,%rdx
   158a4:	48 8d 85 a0 fe ff ff 	lea    -0x160(%rbp),%rax
   158ab:	48 89 d6             	mov    %rdx,%rsi
   158ae:	48 89 c7             	mov    %rax,%rdi
   158b1:	e8 2a 09 00 00       	call   161e0 <_ZNSt6vectorIN6AsmJit5GPVarESaIS1_EEixEm>
   158b6:	48 89 c1             	mov    %rax,%rcx
   158b9:	48 8d 45 c0          	lea    -0x40(%rbp),%rax
   158bd:	ba 00 00 00 00       	mov    $0x0,%edx
   158c2:	48 89 ce             	mov    %rcx,%rsi
   158c5:	48 89 c7             	mov    %rax,%rdi
   158c8:	e8 93 ed ff ff       	call   14660 <_ZN6AsmJitL3ptrERKNS_5GPVarEl>
   158cd:	8b 85 70 fe ff ff    	mov    -0x190(%rbp),%eax
   158d3:	48 63 d0             	movslq %eax,%rdx
   158d6:	48 8d 85 a0 fe ff ff 	lea    -0x160(%rbp),%rax
   158dd:	48 89 d6             	mov    %rdx,%rsi
   158e0:	48 89 c7             	mov    %rax,%rdi
   158e3:	e8 f8 08 00 00       	call   161e0 <_ZNSt6vectorIN6AsmJit5GPVarESaIS1_EEixEm>
   158e8:	48 89 c1             	mov    %rax,%rcx
   158eb:	48 8d 55 c0          	lea    -0x40(%rbp),%rdx
   158ef:	48 8d 85 c0 fe ff ff 	lea    -0x140(%rbp),%rax
   158f6:	48 89 ce             	mov    %rcx,%rsi
   158f9:	48 89 c7             	mov    %rax,%rdi
   158fc:	e8 05 06 00 00       	call   15f06 <_ZN6AsmJit18CompilerIntrinsics3movERKNS_5GPVarERKNS_3MemE>
   15901:	83 85 70 fe ff ff 01 	addl   $0x1,-0x190(%rbp)
   15908:	8b 85 70 fe ff ff    	mov    -0x190(%rbp),%eax
   1590e:	48 98                	cltq   
   15910:	48 39 85 58 fe ff ff 	cmp    %rax,-0x1a8(%rbp)
   15917:	0f 8f 77 ff ff ff    	jg     15894 <_ZL14chase_pointersxxxxxi+0x2e2>
```
该主循环中不包括函数访存9次。下面逐个分析每个函数中的访存次数。
2. mov前置函数1：
```asm
00000000000161e0 <_ZNSt6vectorIN6AsmJit5GPVarESaIS1_EEixEm>:
   161e0:	f3 0f 1e fa          	endbr64 
   161e4:	55                   	push   %rbp
   161e5:	48 89 e5             	mov    %rsp,%rbp
   161e8:	48 89 7d f8          	mov    %rdi,-0x8(%rbp)
   161ec:	48 89 75 f0          	mov    %rsi,-0x10(%rbp)
   161f0:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
   161f4:	48 8b 10             	mov    (%rax),%rdx
   161f7:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
   161fb:	48 c1 e0 05          	shl    $0x5,%rax
   161ff:	48 01 d0             	add    %rdx,%rax
   16202:	5d                   	pop    %rbp
   16203:	c3                   	ret  
```
该函数中访存5次。
2. mov前置函数2：
```asm
0000000000014660 <_ZN6AsmJitL3ptrERKNS_5GPVarEl>:
   14660:	55                   	push   %rbp
   14661:	48 89 e5             	mov    %rsp,%rbp
   14664:	48 83 ec 30          	sub    $0x30,%rsp
   14668:	48 89 7d e8          	mov    %rdi,-0x18(%rbp)
   1466c:	48 89 75 e0          	mov    %rsi,-0x20(%rbp)
   14670:	48 89 55 d8          	mov    %rdx,-0x28(%rbp)
   14674:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
   1467b:	00 00 
   1467d:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
   14681:	31 c0                	xor    %eax,%eax
   14683:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
   14687:	48 8b 55 d8          	mov    -0x28(%rbp),%rdx
   1468b:	48 8b 75 e0          	mov    -0x20(%rbp),%rsi
   1468f:	b9 00 00 00 00       	mov    $0x0,%ecx
   14694:	48 89 c7             	mov    %rax,%rdi
   14697:	e8 3f c0 01 00       	call   306db <_ZN6AsmJit12_MemPtrBuildERKNS_5GPVarElj>
   1469c:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
   146a0:	64 48 2b 04 25 28 00 	sub    %fs:0x28,%rax
   146a7:	00 00 
   146a9:	74 05                	je     146b0 <_ZN6AsmJitL3ptrERKNS_5GPVarEl+0x50>
   146ab:	e8 90 ee ff ff       	call   13540 <__stack_chk_fail@plt>
   146b0:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
   146b4:	c9                   	leave  
   146b5:	c3                   	ret  
```
该函数中访存9次，不包括下一级调用的函数，处于简洁考虑此处不再列出全部下一级函数，但可以确定的是仅次级函数`_ZN6AsmJit3MemC1ERKNS_5GPVarElj`就访存了至少41次，结合以上统计的访存次数，已经超出了DMP预取器的最大窗口。

**结论**
1. 由于AsmJit的大量访存操作，DMP预取器无法生成differential match。
2. 考虑到AsmJit对pChase以及本项目实际已经没有作用，建议使用常规C语言重写指针追踪部分。
3. 如果一定要使用AsmJit在本项目中，必须大大增加DMP预取器的侦听窗口。