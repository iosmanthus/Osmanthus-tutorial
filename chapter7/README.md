# Chapter 7 -- 定时器编程

定时器中断是一个周期性的中断, 每隔一段时间, 一个定时器中断就会到来, 定时器中断在任务调度上非常关键, 所以我们要设计出对其设置的接口.

具体硬件的知识我就不讲了, 我们把它当作黑匣子

```c
#define INPUT_FREQ 0x1234dc

void init_timer( u32 frequency, KInterruptHandler handler )
{
  kreg_int_handler( IRQ0, handler );

  u32 divisor = INPUT_FREQ / frequency;
  kout( 0x43, 0x36, KBYTE );

  u8 l = divisor & 0xff;
  u8 h = ( divisor >> 8 ) & 0xff;

  kout( 0x40, l, KBYTE );
  kout( 0x40, h, KBYTE );
}
```

通过上边的函数, 我们就能够让定时器工作在指定的频率上, 并且注册`handler`, 让其变成定时器中断的处理函数.

也就是说每个时间周期我们都会执行一次中断处理函数, 注意如果中断处理函数执行的时间过长, 也会被下一个定时器中断中断.

我们这里先不介绍锁的概念, 我们这里可以用较为简单的函数, 并且通过降低定时器工作频率的方法来实现测试.

```c
#include <kdebug.h>
#include <kgdt.h>
#include <kidt.h>
#include <kio.h>
#include <kisr.h>
#include <kmultiboot.h>
#include <kpit.h>
#include <ktypes.h>

void hlt() {
  while (1)
    __asm__ volatile("hlt");
}

void timer_handler(KPTRegs *pt_regs) {
  static u32 tick_tock = 0;
  kcprintf(VGA_BLACK, VGA_LIGHT_BROWN, "tick: %u\n", tick_tock++); // 每5ms打印一次
}

void kmain(KMultiBoot *mb) {
  KERNEL_BOOT_INFO = mb;

  KGDTPtr gdt_ptr = kinit_gdt();
  kload_gdt(&gdt_ptr);

  KIDTPtr idt_ptr = kinit_idt();
  kload_idt(&idt_ptr);

  kinit_timer(200, timer_handler);

  __asm__ volatile("sti");

  kprintf("hello world?\n");

  hlt();
}
```

运行结果:

