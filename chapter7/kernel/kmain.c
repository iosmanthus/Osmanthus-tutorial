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
  kcprintf(VGA_BLACK, VGA_LIGHT_BROWN, "tick: %u\n", tick_tock++);
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
