/**
 * @file pat.c
 * @brief Page Attribute Table setup.
 *
 * x86 picks a page's memory type from
 *   IA32_PAT[ (PTE.PAT << 2) | (PTE.PCD << 1) | PTE.PWT ]
 * where PTE.PAT is bit 7 of a 4 KiB page-table entry (@ref PAGE_PAT).
 *
 * The power-on PAT is  PA0..PA7 = WB WT UC- UC WB WT UC- UC.  We change slot 4
 * -- reached with PTE bit 7 set and PCD = PWT = 0 -- from WB to write-combining
 * and leave every other slot alone, so no existing mapping changes meaning and
 * only pages tagged @ref PAGE_WC get the new type.
 *
 * Effective type (SDM vol.3 tbl. 11-7): PAT WC wins over an MTRR of UC or WB,
 * so the framebuffer ends up WC no matter how the firmware left its MTRRs.
 *
 * PAT is per-CPU: @ref pat_init runs on the BSP (before the framebuffer is
 * mapped) and on every AP (top of @c ap_main, before its first klogf paints
 * a character through the console).
 */
#include <pat.h>
#include <cpu.h>


#define IA32_PAT 0x277

/* PAT memory-type encodings. */
#define PA_UC       0x00
#define PA_WC       0x01
#define PA_WT       0x04
#define PA_WB       0x06
#define PA_UC_MINUS 0x07

/* Default layout with slot 4 flipped WB -> WC. */
#define PAT_LO  (PA_WB | (PA_WT << 8) | (PA_UC_MINUS << 16) | (PA_UC << 24))
#define PAT_HI  (PA_WC | (PA_WT << 8) | (PA_UC_MINUS << 16) | (PA_UC << 24))

static int pat_ok;

int pat_available(void) { return pat_ok; }

void pat_init(void) {
    uint32_t edx = 0;
    cpuid(1, 0, 0, 0, &edx);
    if (!(edx & (1u << 16))) {           /* CPUID.01H:EDX[16] = PAT */
        pat_ok = 0;
        return;
    }

    /* SDM change sequence, trimmed for early boot: serialise, flush caches and
     * the TLB around the MSR write so no line keeps a stale type. Cheap here --
     * on the BSP the framebuffer is not mapped yet; on an AP nothing is dirty. */
    asm volatile(
        "wbinvd\n\t"
        "mov %%cr3, %%eax\n\t"
        "mov %%eax, %%cr3\n\t"
        ::: "eax", "memory");

    wrmsr(IA32_PAT, PAT_LO, PAT_HI);

    asm volatile(
        "wbinvd\n\t"
        "mov %%cr3, %%eax\n\t"
        "mov %%eax, %%cr3\n\t"
        ::: "eax", "memory");

    pat_ok = 1;
}
