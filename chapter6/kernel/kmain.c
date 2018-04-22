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
