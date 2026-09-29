/**
 * @file elf.c
 * @brief 32-bit ELF loader: stages the file at a fixed load address, validates
 *        the header and copies each PT_LOAD segment to its virtual address in
 *        the target process's page directory.
 */


#include <vfs.h>
#include <lib/string.h>
#include <elf.h>
#include <printf.h>

/**
 * The ELF staging window: the kernel directory own scratch area for the
 * raw file bytes, read here before load_elf_relocate() copies each PT_LOAD
 * segment to its real, linked virtual address (every app own link.lds
 * uses `. = 8M`, i.e. KERNEL_SPACE_END -- see mm.h). This window used to be
 * [0x700000, 0x800000) (1 MiB, ending exactly at KERNEL_SPACE_END), which
 * was enough for every app until a full real Qt6 + FreeType + HarfBuzz +
 * PCRE2 graphical closure needed staging room two orders of magnitude
 * bigger than anything before it.
 *
 * vmm_map() (used below) allocates a fresh physical frame from the general
 * pool for each staged page -- it is not an identity mapping, so this
 * window location is not tied to any specific physical memory the way
 * the KERNEL_SPACE_END low region is. It only needs to be a virtual
 * range the kernel directory itself never uses for anything else, since
 * every kernel *thread* (not user process -- those get their own, separate
 * page directory) runs on that same shared directory (see the sched.c and
 * proc.c comments on why their stacks were relocated away from
 * KERNEL_SPACE_END for exactly this reason).
 *
 * [72 MiB, 96 MiB) is genuinely free for this: the mm.h comment already
 * documents the per-process ceiling (image at 8 MiB plus PROC_HEAP_MAX,
 * 64 MiB = 72 MiB) and where the kernel heap starts (KHEAP_BASE, 96 MiB)
 * as the two numbers either side of this gap, and neither a user process
 * own heap (a different page directory entirely) nor the kernel heap
 * itself ever maps anything here. 24 MiB of real headroom, not just barely
 * enough for today closure.
 */
#define MEMORY_LOAD_ADDRESS 0x04800000u
#define MEMORY_LOAD_WINDOW_SIZE (24u * 1024u * 1024u)

/**
 * Checks if the file can be executed in this OS
 */
int elf_validate(elf_header_t *eh) {
    if(eh == 0)
        return 0;

    if(eh->magic != ELF_MAGIC_NUMBER) {
        printf("Magic number wrong\n");
        return 0;
    }
    
    if(eh->instruction_set != ELF_X86) {
        printf("Not an x86 executable\n");
        return 0;
    }
    
    if(eh->arch != ELF_32BIT) {
        printf("Not a 32 bit executable\n");
        return 0;
    }
    
    if(eh->machine != ELF_LITTLE_ENDIAN) {
        printf("Not a little endian executable\n");
        return 0;
    }
    
    if(eh->elf_version2 != 1) {
        printf("Wrong ELF version\n");
        return 0;
    }
    return 1;
}

/**
 * Loads an ELF executable in memory and partially builds threads' info
 */
int load_elf(char *name, thread_t *thread, page_dir_t *pdir) {
    // Load the file into memory
    uint32_t file_size = load_elf_file(name);
    if(!file_size) {
        printf("Error loading file\n");
        return 0;
    }
    
    // Check the elf header
    elf_header_t *eh = (elf_header_t *) MEMORY_LOAD_ADDRESS;
    if(!elf_validate(eh)) {
        printf("Failed validating elf\n");
        return 0;
    }
    
    // Relocate executable parts
    if(!load_elf_relocate(thread, pdir, eh)) {
        printf("Error relocating\n");
        return 0;
    }
    
    // Unmap executable from kernel directory
    for(uint32_t i = 0; i < file_size; i++) {
        vmm_unmap(get_kern_directory(), (uint32_t) MEMORY_LOAD_ADDRESS + (i * PAGE_SIZE));
    }
    
    return 1;
}

/**
 * Moves the file from the disk to RAM
 */
int load_elf_file(char *name) {
    // Open the executable
    file *f = vfs_file_open(name, "r");
    if((f->type == FS_NULL) || (f->type == FS_DIR)) {
        printf("Failed opening file\n");
        return 0;
    }
    
    // Load the executable in memory
    uint32_t j = 0;
    int file_size = 0;
    while(f->eof != 1) {
        // The staging window is [MEMORY_LOAD_ADDRESS, MEMORY_LOAD_ADDRESS +
        // MEMORY_LOAD_WINDOW_SIZE). Refuse a file that would spill past it
        // rather than corrupting memory.
        if((uint32_t) (j * 512) >= MEMORY_LOAD_WINDOW_SIZE) {
            printf("elf: file too large for the load window\n");
            // Undo the vmm_map() calls the loop below already made for this
            // rejected file - same unmap load_elf() does after a successful
            // load, just run here instead since we bail out before that point.
            // Left unmapped, these pages (and the page tables backing them)
            // are never freed: a process only has to `run` an oversized file a
            // few times to exhaust the page-table pool and panic the kernel.
            for(uint32_t p = 0; p < (uint32_t) file_size; p++)
                vmm_unmap(get_kern_directory(), (uint32_t) MEMORY_LOAD_ADDRESS + (p * PAGE_SIZE));
            vfs_file_close(f);
            return 0;
        }
        // The executable needs more memory, so reserve it
        if(((j + 8) % 8) == 0) {
            if(!vmm_map(get_kern_directory(), (uint32_t) MEMORY_LOAD_ADDRESS + (j * 512), PAGE_PRESENT | PAGE_RW)) {
                printf("Error mapping memory");
                return -1;
            }
            file_size++;
        }
        // Copy the file into memory
        vfs_file_read(f, (char *) MEMORY_LOAD_ADDRESS + (j * 512));
        j++;
    }
    vfs_file_close(f);

    return file_size;
}

/**
 * Moves the executable parts to the correct virtual address for execution
 */
int load_elf_relocate(thread_t *thread, page_dir_t *pdir, elf_header_t *eh) {
    elf_program_header_t *ph = (elf_program_header_t *) ((uint32_t) eh + eh->program_header);
    thread->eip = eh->entry;

    uint32_t img_lo = 0xFFFFFFFF, img_hi = 0;

    for(uint32_t i = 0; i < eh->entry_number_prog_header; i++) {
        if(ph[i].p_type != 1 || ph[i].p_mem_size == 0)   /* PT_LOAD only */
            continue;

        /* The span the segment occupies in memory: from its page-aligned base
         * up to p_vaddr + p_mem_size (p_mem_size, not p_file_size, so the .bss
         * tail past the file bytes is mapped too). */
        uint32_t seg_lo = ph[i].p_vaddr & ~(PAGE_SIZE - 1);
        uint32_t seg_hi = ph[i].p_vaddr + ph[i].p_mem_size;

        if(seg_lo < img_lo) img_lo = seg_lo;
        if(seg_hi > img_hi) img_hi = seg_hi;

        /* Map every page of the span - each to its OWN fresh frame - in the
         * kernel directory (so we can fill it now, while that directory is
         * active) and in the target process directory. The old code mapped
         * every page of a segment onto the single frame backing its first
         * page, so any program larger than one page per segment was corrupt. */
        for(uint32_t va = seg_lo; va < seg_hi; va += PAGE_SIZE) {
            /* The image is mapped at the program's own link address into the
             * kernel directory, so it can be filled while that directory is
             * active - but every kernel thread is also running on that
             * directory. If the range is already mapped there it belongs to
             * something else, and vmm_map() would silently repoint it and then
             * memcpy the file over whatever was living there (a running
             * thread's stack, for instance). Say so instead of corrupting it. */
            if(get_phys_addr(get_kern_directory(), va)) {
                printf("elf: segment va %x already mapped in the kernel "
                       "directory - refusing to load over it\n", va);
                return 0;
            }
            if(!vmm_map(get_kern_directory(), va, PAGE_PRESENT | PAGE_RW) ||
               !vmm_map_phys(pdir, va,
                             (uint32_t) get_phys_addr(get_kern_directory(), va),
                             PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
                printf("elf: out of memory mapping segment at %x\n", va);
                return 0;
            }
        }

        memcpy((void *) ph[i].p_vaddr,
               (void *) ((uint32_t) MEMORY_LOAD_ADDRESS + ph[i].p_offset),
               ph[i].p_file_size);
        /* Zero the partial last page of .data and all of .bss. */
        memset((void *) (ph[i].p_vaddr + ph[i].p_file_size), 0,
               seg_hi - (ph[i].p_vaddr + ph[i].p_file_size));

        for(uint32_t va = seg_lo; va < seg_hi; va += PAGE_SIZE)
            vmm_unmap_phys(get_kern_directory(), va);
    }

    if(img_lo == 0xFFFFFFFF) {
        printf("elf: no loadable segments\n");
        return 0;
    }

    thread->image_base = img_lo;
    thread->image_size = (img_hi - img_lo + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    return 1;
}
