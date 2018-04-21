#include <kdebug.h>
#include <kgdt.h>
#include <kio.h>
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

  kprintf("hello world?\n");

  hlt();
}
