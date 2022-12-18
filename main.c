#include <log.h>
#include <uart.h>
#include <assert.h>

#include <kconsole.h>
#include <multiboot.h>
#include <multiboot2.h>
#include <io.h>
#include <cpu.h>

#include <memory.h>

#include <bfb.h>
#include <video.h>
#include <vga.h>

#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <pit.h>

#include <keyboard.h>
#include <mouse.h>

#include <syscall.h>
#include <tss.h>

#include <sched.h>

#include <rtc.h>
#include <pcspk.h>

#include <vfs.h>

#include <ata.h>
#include <floppy.h>

#include <fpu.h>
#include <sound.h>

#include <pci.h>

void floppy_detect() {
    unsigned char a, b, c;
    outportb(0x70, 0x10);
    c = inportb(0x71);

    a = c >> 4;  // Get high nibble
    b = c & 0xF; // Get low nibble by ANDing out low nibble

    static char *drive_type[6] = {
        "no floppy drive", "360KB 5.25\" floppy drive", 
        "1.2MB 5.25\" floppy drive", "720KB 3.5\"",
        "1.44MB 3.5\"", "2.88MB 3.5\""};

    klogf(LOG_INFO, "Floppy drive A is a: %s\n", drive_type[a]);
    klogf(LOG_INFO, "Floppy drive B is a: %s\n", drive_type[b]);
}

static const char *e820names[] = {
    "invalid",
    "available",
    "reserved",
    "acpi",
    "nvs",
    "unusable"
};

extern uint32_t multiboot2_mem_size;

void kernel_main(unsigned long magic, unsigned long addr) {
    unsigned size = *(unsigned *) addr;
    (void)size;
    (void)e820names;

    uint64_t tsc = rdtsc();
    
    uart_init();
    kconsole = &uartdev;
    
    if (magic == MULTIBOOT_LOADER_MAGIC) {
        klogf(LOG_INFO, "MultiBoot 1 addr: 0x%lx magic: 0x%x size: 0x%x\n",
          (uintptr_t)addr, (unsigned) magic, size);
        
        multiboot_info_parse((const multiboot_info_t *)addr);
        multiboot_info_t *info = (multiboot_info_t *)addr;
        pmm_init(info->mem_upper + info->mem_lower);
    } else if (magic == MULTIBOOT2_LOADER_MAGIC) {
        klogf(LOG_INFO, "MultiBoot 2 addr: 0x%lx magic: 0x%x size: 0x%x\n",
          (uintptr_t)addr, (unsigned) magic, size);
        
        multiboot2_info_parse((const multiboot2_info_t *)addr);
        pmm_init(multiboot2_mem_size);
    } else {
        klogf(LOG_EMERG, "Error: no multiboot, magic: 0x%lx. Exiting.", magic);
        exit_qemu(1);
    }
    
    for (int i = 0; i < e820counter; i++) {
        struct e820memmap map = e820table[i];
        
        // Check if the memory region is available
        if (map.type == 1) {
            klogf(LOG_INFO, " BIOS-e820 [addr: 0x%x%x, size: 0x%x%x] %s\n",
              (unsigned) (map.base_address >> 32),
              (unsigned) (map.base_address & 0xffffffff),
              (unsigned) (map.size >> 32),
              (unsigned) (map.size & 0xffffffff),
              e820names[(unsigned) map.type]);
            
            pmm_init_reg(map.base_address & 0xffffffff, map.size & 0xffffffff);
        }
    }
        
    pmm_init2();
    
    vmm_init();
    kheap_init();

    if (!bfb_addr) {
        vga_init();
    } else {
        vbe_init();
    }
    
    gdt_init();
    idt_init(0x8);
    fpu_init();
    pic_init(0x20, 0x28);
    pit_init();
    pit_start_counter(1000, PIT_COUNTER_0, PIT_MODE_SQUAREWAVEGEN);

    vfs_init();
    //ata_init();
    floppy_detect();
    //floppy_init(); // requires irqs to be enabled
    
    keyboard_init();
    mouse_init();
    uart_rx_ir();
    sound_init();
    
    syscall_init();
    install_tss();

    rtc_init();

    pci_test();

    klogf(LOG_INFO, "Initialization took: %llu\n", rdtsc() - tsc);

    sched_init();
    
    while(1) halt();
}
