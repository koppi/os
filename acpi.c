/**
 * @file acpi.c
 * @brief RSDP scan, RSDT walk and MADT parse for LAPIC discovery.
 *
 * The ACPI tables live in E820 type-3 memory above the low 4 MiB identity map,
 * so @ref acpi_map maps them 1:1 on demand (the same trick e1000.c uses for its
 * MMIO BAR). If the tables are absent or malformed we fall back to a single
 * CPU, which keeps `-smp 1` working unchanged.
 */
#include <acpi.h>
#include <paging.h>
#include <mm.h>

#include <log.h>

/* --- ACPI table signatures (RSDT entries are 4-byte physical addresses). --- */
#define RSDP_SIG     "RSD PTR "
#define MADT_SIG     "APIC"

/* --- MADT entry types (the only ones we care about). --- */
#define MADT_LAPIC   0
#define MADT_LAPIC_BASE 5

/* Reserving enough room for up to MAX_ACPI_CPU local APIC records. */
#define MAX_CPU 8

typedef struct __attribute__((packed)) {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t  ext_checksum;
    uint8_t  reserved[3];
} rsdp_t;

typedef struct __attribute__((packed)) {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} sdt_t;

typedef struct __attribute__((packed)) {
    sdt_t    header;
    uint32_t entry[];       /* 32-bit physical addresses of other SDTs. */
} rsdt_t;

typedef struct __attribute__((packed)) {
    sdt_t    header;
    uint32_t lapic_addr;    /* Physical base of the local APIC. */
    uint32_t flags;
} madt_t;

/* Per-CPU LAPIC id table from the MADT. */
typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  length;
    uint8_t  acpi_processor_id;
    uint8_t  apic_id;
    uint32_t flags;
} madt_lapic_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
    uint8_t  reserved;
    uint8_t  efi_id;
    uint32_t lapic_addr;
    uint32_t global_int_base;
} madt_lapic_base_t;

static int      acpi_found      = 0;
static uint32_t cpu_apicids[MAX_CPU];
static int      cpu_count       = 0;
static uint32_t lapic_base      = 0xFEE00000;   /* x86 default. */
static uint32_t bsp_apicid      = 0;
static const void *g_madt       = 0;            /* mapped MADT, for the IOAPIC code */
static const void *g_rsdp       = 0;            /* mapped, validated RSDP */

/**
 * @brief Identity-map a physically-located ACPI table so it can be read.
 *        Tables sit above the low-4 MiB map, hence the on-demand 1:1 mapping.
 */
static void *acpi_map(uint32_t phys, uint32_t len) {
    uint32_t end = (phys + len + 0xFFF) & ~0xFFFu;
    for (uint32_t va = phys & ~0xFFFu; va < end; va += PAGE_SIZE) {
        if (get_phys_addr(get_kern_directory(), va) == 0) {
            if (!vmm_map_phys(get_kern_directory(), va, va, PAGE_PRESENT | PAGE_RW))
                return 0;
        }
    }
    return (void *) phys;
}

/** @brief Byte-compare @p n bytes (no libc memcmp in the kernel). */
static int bytecmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = (const uint8_t *) a, *q = (const uint8_t *) b;
    for (size_t i = 0; i < n; i++)
        if (p[i] != q[i])
            return 1;
    return 0;
}

/** @brief Check/repair the ACPI checksum of @p tbl (sum of bytes must be 0). */
static int checksum_ok(const uint8_t *p, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++)
        sum += p[i];
    return sum == 0;
}

/** Physical address of the RSDP GRUB handed us via a multiboot2 tag (0 if none).
 *  Set by the multiboot2 parser; the only way to find the RSDP under UEFI. */
extern uint32_t multiboot2_acpi_rsdp;

/** @brief Validate a candidate RSDP at @p addr. @return the mapped struct or 0. */
static const rsdp_t *check_rsdp(uint32_t addr) {
    const rsdp_t *r = (const rsdp_t *) acpi_map(addr, sizeof(rsdp_t));
    if (!r)
        return 0;
    if (bytecmp(r->signature, RSDP_SIG, 8) != 0)
        return 0;
    if (!checksum_ok((const uint8_t *) r, 20))          /* v1 checksum */
        return 0;
    if (r->revision >= 2 && !checksum_ok((const uint8_t *) r, 36))
        return 0;                                       /* v2 extended checksum */
    return r;
}

/** @brief Validate the RSDP and return it if usable, else NULL. */
static const rsdp_t *find_rsdp(void) {
    const rsdp_t *r;

    /* 1. The pointer GRUB copied in from the EFI system table, if we booted
     *    via UEFI. The legacy scan below finds nothing there. */
    if (multiboot2_acpi_rsdp && (r = check_rsdp(multiboot2_acpi_rsdp)))
        return r;

    /* 2. Legacy BIOS: the RSDP lives in the EBDA (first KiB) or 0xE0000-0xFFFFF,
     *    on a 16-byte boundary. */
    for (uint32_t addr = 0xE0000; addr < 0x100000; addr += 16)
        if ((r = check_rsdp(addr)))
            return r;

    /* EBDA: paragraph pointer at 0x40E, then scan the first 1 KiB. */
    uint16_t ebda_seg;
    __asm__("movw (%1), %0" : "=r" (ebda_seg) : "r" (acpi_map(0x40E, 2)));
    uint32_t ebda = (ebda_seg << 4) & 0xFFFFF;
    for (uint32_t addr = ebda; addr < ebda + 1024; addr += 16)
        if ((r = check_rsdp(addr)))
            return r;

    return 0;
}

/** @brief Parse the MADT, filling cpu_apicids / cpu_count / lapic_base. */
static void parse_madt(const madt_t *madt) {
    lapic_base = madt->lapic_addr;
    klogf(LOG_INFO, "acpi: MADT lapic base 0x%x\n", (unsigned) lapic_base);

    const uint8_t *p = (const uint8_t *) (madt) + sizeof(madt_t);
    const uint8_t *end = (const uint8_t *) madt + madt->header.length;

    /* The first reported CPU with the "enabled" flag set is the BSP; the rest
     * are application processors. We record enabled LAPICs only. */
    while (p < end && cpu_count < MAX_CPU) {
        uint8_t type = p[0];
        uint8_t len  = p[1];
        if (len < 2)
            break;
        if (type == MADT_LAPIC) {
            madt_lapic_t *lap = (madt_lapic_t *) p;
            if (lap->flags & 0x1) {
                if (cpu_count == 0)
                    bsp_apicid = lap->apic_id;
                cpu_apicids[cpu_count++] = lap->apic_id;
            }
        } else if (type == MADT_LAPIC_BASE) {
            madt_lapic_base_t *b = (madt_lapic_base_t *) p;
            lapic_base = b->lapic_addr;
        }
        p += len;
    }
}

typedef struct __attribute__((packed)) {
    sdt_t    header;
    uint64_t entry[];       /* 64-bit physical addresses of other SDTs. */
} xsdt_t;

/**
 * @brief Locate an ACPI table by signature, walking the XSDT (ACPI 2.0+) when
 *        the RSDP advertises it, else the 32-bit RSDT.
 */
static sdt_t *acpi_find_table(const rsdp_t *rsdp, const char *sig) {
    if (rsdp->revision >= 2 && rsdp->xsdt_addr &&
        (rsdp->xsdt_addr >> 32) == 0) {
        xsdt_t *xsdt = (xsdt_t *) acpi_map((uint32_t) rsdp->xsdt_addr, sizeof(sdt_t));
        if (!xsdt) return 0;
        if (bytecmp(xsdt->header.signature, "XSDT", 4) == 0) {
            xsdt = (xsdt_t *) acpi_map((uint32_t) rsdp->xsdt_addr, xsdt->header.length);
            if (!xsdt) return 0;
            uint32_t count = (xsdt->header.length - sizeof(sdt_t)) / 8;
            for (uint32_t i = 0; i < count; i++) {
                uint64_t e = xsdt->entry[i];
                if (e == 0 || (e >> 32) != 0)
                    continue;
                sdt_t *tbl = (sdt_t *) acpi_map((uint32_t) e, sizeof(sdt_t));
                if (!tbl) continue;
                if (bytecmp(tbl->signature, sig, 4) == 0) {
                    sdt_t *result = (sdt_t *) acpi_map((uint32_t) e, tbl->length);
                    return result;
                }
            }
        }
    }

    if (rsdp->rsdt_addr) {
        rsdt_t *rsdt = (rsdt_t *) acpi_map(rsdp->rsdt_addr, sizeof(sdt_t));
        if (!rsdt) return 0;
        if (bytecmp(rsdt->header.signature, "RSDT", 4) == 0) {
            rsdt = (rsdt_t *) acpi_map(rsdp->rsdt_addr, rsdt->header.length);
            if (!rsdt) return 0;
            uint32_t count = (rsdt->header.length - sizeof(sdt_t)) / 4;
            for (uint32_t i = 0; i < count; i++) {
                if (!rsdt->entry[i])
                    continue;
                sdt_t *tbl = (sdt_t *) acpi_map(rsdt->entry[i], sizeof(sdt_t));
                if (!tbl) continue;
                if (bytecmp(tbl->signature, sig, 4) == 0) {
                    sdt_t *result = (sdt_t *) acpi_map(rsdt->entry[i], tbl->length);
                    return result;
                }
            }
        }
    }
    return 0;
}

/**
 * @brief Locate the MADT (via XSDT/RSDT) and parse it.
 */
static void locate_madt(const rsdp_t *rsdp) {
    sdt_t *madt = acpi_find_table(rsdp, MADT_SIG);
    if (madt) {
        g_madt = madt;
        parse_madt((madt_t *) madt);
    }
}

/** @return The mapped MADT (ACPI "APIC" table), or 0 if none was found. */
const void *acpi_madt(void) { return g_madt; }

/** @return The validated RSDP, or 0. */
const void *acpi_rsdp(void) { return g_rsdp; }

void acpi_init(void) {
    const rsdp_t *rsdp = find_rsdp();
    if (!rsdp) {
        klogf(LOG_WARN, "acpi: no RSDP found, assuming 1 CPU\n");
        cpu_count = 1;
        cpu_apicids[0] = 0;
        return;
    }
    g_rsdp = rsdp;
    locate_madt(rsdp);
    if (cpu_count == 0) {
        klogf(LOG_WARN, "acpi: no MADT CPUs found, assuming 1 CPU\n");
        cpu_count = 1;
        cpu_apicids[0] = 0;
    }
    acpi_found = 1;
    klogf(LOG_INFO, "acpi: MADT %d CPU(s)\n", cpu_count);
}

int acpi_cpu_count(void) { return cpu_count; }

uint32_t acpi_cpu_apicid(int i) {
    if (i < 0 || i >= cpu_count)
        return 0;
    return cpu_apicids[i];
}

uint32_t acpi_lapic_base(void) { return lapic_base; }

uint32_t acpi_bsp_apicid(void) { return bsp_apicid; }
