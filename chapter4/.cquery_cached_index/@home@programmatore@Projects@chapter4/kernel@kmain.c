#include <kdebug.h>
#include <kmultiboot.h>
#include <ktypes.h>
#include <kvga.h>

void hlt() {
  kstack_trace();
  kpanic("Error?!\n");
  while (1)
    __asm__ volatile("hlt");
}

void kmain(KMultiBoot *mb) {
  KERNEL_BOOT_INFO = mb;
  hlt();
}
