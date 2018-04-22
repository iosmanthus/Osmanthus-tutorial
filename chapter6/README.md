# Chapter 6 -- 设置中断向量表

中断的具体概念在计算机组成原理课程中已经有介绍了.

通俗来讲, 中断是CPU的一种通知机制. 中断的引入其实就是为了让工作速度较快的CPU在向外部设备发出请求时不需要一直等待等待其反馈结果, 从而提高我们CPU的工作效率.

当某个中断到来时,  典型的处理方式就是, CPU打断当前的任务, 保护好现场, 然后再转移到相应的中断处理函数当中去.  处理完之后, 中断处理函数可以选择返回或者是不返回. 假若中断处理函数返回了, CPU就会恢复现场, 继续被打断的执行流, 当然这里执行流重新开始的位置跟中断类型有关, 这里有兴趣的可以去看看`CSAPP`第八章的解释. 这里就不再赘述了.

我们接下来要做的就是设置好中断向量表(IDT), IDT的作用就是, 当中断到来的时候, CPU根据中断号, 在表中找到相关表项, 然后根据里边的中断服务程序地址跳转到服务程序中去.



首先, 我们还是要定义IDT表项

```c
typedef struct _KInterrputGate {
  u16 base_low;
  u16 segment_selector; // Segment selector
  u8 reserve;           // High 4 bits are all 0s
  u8 flags;
  u16 base_high;
} __attribute__((packed)) KInterrputGate;
```

然后是跟之前GDT一样, 定义一个IDT指针. 同样也是48位的.

```c
typedef struct _KIDTPtr {
  u16 limit;
  u32 base;
} __attribute__((packed)) KIDTPtr;
```

接下来跟之前的GDT类似, 我们逐个设置表项. 但是在这之前我们要重新映射IRQ Table, 这里的IRQ是指中断号>=32的中断

我们可以通过这样来实现:

```c
#include <kgdt.h>
#include <kidt.h>
#include <kisr.h>
#include <kport.h>

#define ISR_INIT(id)                                                           \
  do {                                                                         \
    set_interrupt_gate(&idt[(id)], (u32)isr##id, GDT_CODE_SELECTOR, 0x8e);     \
  } while (0);

#define IRQ_INIT(id)                                                           \
  do {                                                                         \
    set_interrupt_gate(&idt[(id + 32)], (u32)irq##id, GDT_CODE_SELECTOR,       \
                       0x8e);                                                  \
  } while (0);

static KInterrputGate idt[IDT_ENTRY_CNT];
static int idt_init;

static void set_interrupt_gate(KInterrputGate *intgt, u32 base,
                               u16 segment_selector, u8 flags);

void set_interrupt_gate(KInterrputGate *intgt, u32 base, u16 segment_selector,
                        u8 flags) {
  // 这里具体的意义可以查看Intel man, 其实可以忽略这些参数, 当作黑匣子
  intgt->base_low = base & 0xffff;
  intgt->segment_selector = segment_selector;
  intgt->reserve = 0x0;
  intgt->flags = flags; // 10001110b
  intgt->base_high = (base >> 16) & 0xffff;
}

KIDTPtr kinit_idt() {
  static KIDTPtr idt_ptr;
  if (!idt_init) {
    idt_ptr.base = (u32)&idt;
    idt_ptr.limit = sizeof(idt) - 1;  // 规定其大小为实际大小 - 1
    idt_init = 1;

    // Remap the irq table.
    kout(0x20, 0x11, KBYTE);
    kout(0xA0, 0x11, KBYTE);
    kout(0x21, 0x20, KBYTE);
    kout(0xA1, 0x28, KBYTE);
    kout(0x21, 0x04, KBYTE);
    kout(0xA1, 0x02, KBYTE);
    kout(0x21, 0x01, KBYTE);
    kout(0xA1, 0x01, KBYTE);
    kout(0x21, 0x0, KBYTE);
    kout(0xA1, 0x0, KBYTE);

    ISR_INIT(0);
    ISR_INIT(1);
    ISR_INIT(2);
    ISR_INIT(3);
    ISR_INIT(4);
    ISR_INIT(5);
    ISR_INIT(6);
    ISR_INIT(7);
    ISR_INIT(8);
    ISR_INIT(9);
    ISR_INIT(10);
    ISR_INIT(11);
    ISR_INIT(12);
    ISR_INIT(13);
    ISR_INIT(14);
    ISR_INIT(15);
    ISR_INIT(16);
    ISR_INIT(17);
    ISR_INIT(18);
    ISR_INIT(19);
    ISR_INIT(20);
    ISR_INIT(21);
    ISR_INIT(22);
    ISR_INIT(23);
    ISR_INIT(24);
    ISR_INIT(25);
    ISR_INIT(26);
    ISR_INIT(27);
    ISR_INIT(28);
    ISR_INIT(29);
    ISR_INIT(30);
    ISR_INIT(31);

    IRQ_INIT(0);
    IRQ_INIT(1);
    IRQ_INIT(2);
    IRQ_INIT(3);
    IRQ_INIT(4);
    IRQ_INIT(5);
    IRQ_INIT(6);
    IRQ_INIT(7);
    IRQ_INIT(8);
    IRQ_INIT(9);
    IRQ_INIT(10);
    IRQ_INIT(11);
    IRQ_INIT(12);
    IRQ_INIT(13);
    IRQ_INIT(14);
    IRQ_INIT(15);

    // System call
    ISR_INIT(255);
  }
  return idt_ptr;
}
```

问题是`isr0`, `irq0`这些还没有被定义, 怎么办呢, 我们使用汇编来定义.

注意这里部分的中断服务程序是会有错误码返回的.

```assembly
;; 这里用到了yasm中的宏功能. 具体用法可以参考其文档
%macro ISR_ERRCODE 1
[global isr%1:function]
isr%1:
  cli
  push dword %1 ; Interrupt id ;这里就自动压入了错误码
  jmp kisr_common_stub
.end
size isr%1 isr%1.end - isr%1
%endmacro

%macro ISR_NOERRCODE 1
[global isr%1:function]
isr%1:
  cli
  push dword 0  ; Dummy error code ; 同样压入一个伪错误码
  push dword %1
  jmp kisr_common_stub
.end
size isr%1 isr%1.end - isr%1
%endmacro

%macro IRQ 2
[global irq%1:function]
irq%1:
  cli
  push dword 0 ; dummy error code
  push dword %2 ; push interrupt id
  jmp kirq_common_stub
.end
size irq%1 irq%1.end - irq%1
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7

ISR_ERRCODE 8

ISR_NOERRCODE 9

ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14

ISR_NOERRCODE 15
ISR_NOERRCODE 16

ISR_ERRCODE 17

ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; System call
ISR_NOERRCODE 255

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47
```

其实从上边的代码我们可以看到, 无论是`irqX`还是`isrX`都只是一个小入口, 处理最基本的操作, 然后跳转至一个`handler`,这个是很重要的设计, 这样我们就可以将响应和处理的细节分开, 有助于我们更换中断的具体服务程序.

我们就来具体看看这些`handler`的入口怎么设计.

```assembly
[extern kisr_handler]
kisr_common_stub:
  pusha

  mov ax, ds

  push eax

  mov ax, 0x10   ; Kernel data segment selector 这里是GDT中的段切换操作.
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  push esp ; 保存栈指针.
  call kisr_handler
  add esp, 4

  pop ebx
  mov ds, bx
  mov es, bx
  mov fs, bx
  mov gs, bx
  mov ss, bx


  popa
  add esp, 8
  iret ;这里是用于中断返回的指令

[extern kirq_handler]
kirq_common_stub:
  pusha

  mov ax, ds

  push eax

  mov ax, 0x10   ; Kernel data segment selector
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  push esp
  call kirq_handler
  add esp, 4

  pop ebx
  mov ds, bx
  mov es, bx
  mov fs, bx
  mov gs, bx
  mov ss, bx


  popa
  add esp, 8
  iret
```

接下来的问题就是具体`handler`的实现了

```c
#include <kdebug.h>
#include <kidt.h>
#include <kio.h>
#include <kisr.h>
#include <kport.h>

static KInterruptHandler int_handler_pool[IDT_ENTRY_CNT] = {NULL};

// Register an interrupt handler function
// 这个地方是很重要的, 我们引入函数注册的机制, 可以很方便的替换中断服务程序.
void kreg_int_handler(u32 int_id, KInterruptHandler handler)
{
  int_handler_pool[int_id] = handler;
}

void kisr_handler(KPTRegs *pt_regs)
{
  // If registered
  KInterruptHandler handler = int_handler_pool[pt_regs->int_id]; 
  // 如果存在已经注册的函数, 跳转.
  if (handler)
    handler(pt_regs);
  else
    kcprintf(VGA_BLACK, VGA_RED, "Unhandled interrupt: %d\n",
		    pt_regs->int_id);
}


void kirq_handler(KPTRegs *pt_regs)
{
  // Send an EOI (end of interrupt) signal to the PICs.
  // If this interrupt involved the slave.
  if (pt_regs->int_id >= 40)
    kout(0xA0, 0x20, KBYTE);

  // Reset master
  kout(0x20, 0x20, KBYTE);
  // 上边的可以当作黑匣子
    
  // kirq_handler 只是一个特例.
  kisr_handler(pt_regs);
}
```

问题是`KPTRegs`作为要保护的现场, 我们还没有定义.

```c
typedef struct _KPTRegs {
  u32 ds; // Save previous data segment

  u32 edi; // From edi to eax, all pushed by instruction: pusha
  u32 esi;
  u32 ebp;
  u32 esp;
  u32 ebx;
  u32 edx;
  u32 ecx;
  u32 eax;

  u32 int_id;     // Interrupt id
  u32 error_code; // Push by CPU

  /* ---- pushed by CPU ---- */
  u32 eip;
  u32 cs;
  u32 eflags;
  u32 useresp;
  u32 ss;
} KPTRegs;
```

接下来就是在`kmain.c`中调用他们了.

```c
#include <kdebug.h>
#include <kgdt.h>
#include <kidt.h>
#include <kio.h>
#include <kisr.h>
#include <kmultiboot.h>
#include <ktypes.h>

void hlt() {
  while (1)
    __asm__ volatile("hlt");
}

void kmain(KMultiBoot *mb) {
  KERNEL_BOOT_INFO = mb;

  KGDTPtr gdt_ptr = kinit_gdt();
  kload_gdt(&gdt_ptr);

  KIDTPtr idt_ptr = kinit_idt();
  kload_idt(&idt_ptr);

  __asm__ volatile("sti");
  kprintf("hello world?\n");

  hlt();
}
```

如果这个时候可以看到不断有未捕捉中断的提示, 说明代码已经可以工作了.