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
#include <lib/string.h>
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

/**
 * @brief Identity-map a physically-located ACPI table so it can be read.
 *        Tables sit above the low-4 MiB map, hence the on-demand 1:1 mapping.
 */
static void *acpi_map(uint32_t phys, uint32_t len) {
    /* Map the full span so reads beyond the first page do not fault. */
    uint32_t end = (phys + len + 0xFFF) & ~0xFFFu;
    for (uint32_t va = phys & ~0xFFFu; va < end; va += PAGE_SIZE) {
        if (get_phys_addr(get_kern_directory(), va) == 0)
            vmm_map_phys(get_kern_directory(), va, va, PAGE_PRESENT | PAGE_RW);
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

/** @brief Validate the RSDP and return it if usable, else NULL. */
static const rsdp_t *find_rsdp(void) {
    /* The RSDP lives either in the EBDA (first KiB) or 0xE0000-0xFFFFF. Scan
     * both on 16-byte boundaries. */
    for (uint32_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        const rsdp_t *r = (const rsdp_t *) acpi_map(addr, sizeof(rsdp_t));
        if (bytecmp(r->signature, RSDP_SIG, 8) == 0 &&
            checksum_ok((const uint8_t *) r, 20))
            return r;
    }
    /* EBDA: paragraph pointer at 0x40E, then scan the first 1 KiB. */
    uint32_t ebda_seg = *(uint16_t *) acpi_map(0x40E, 2);
    uint32_t ebda = (ebda_seg << 4) & 0xFFFFF;
    for (uint32_t addr = ebda; addr < ebda + 1024; addr += 16) {
        const rsdp_t *r = (const rsdp_t *) acpi_map(addr, sizeof(rsdp_t));
        if (bytecmp(r->signature, RSDP_SIG, 8) == 0 &&
            checksum_ok((const uint8_t *) r, 20))
            return r;
    }
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

/**
 * @brief Locate the MADT by walking the RSDT, then parse it.
 */
static void locate_madt(const rsdp_t *rsdp) {
    rsdt_t *rsdt = (rsdt_t *) acpi_map(rsdp->rsdt_addr,
                                       sizeof(sdt_t));
    if (bytecmp(rsdt->header.signature, "RSDT", 4) != 0)
        return;

    uint32_t count = (rsdt->header.length - sizeof(sdt_t)) / 4;
    for (uint32_t i = 0; i < count; i++) {
        sdt_t *tbl = (sdt_t *) acpi_map(rsdt->entry[i], sizeof(sdt_t));
        if (bytecmp(tbl->signature, MADT_SIG, 4) == 0) {
            parse_madt((madt_t *) tbl);
            return;
        }
    }
}

void acpi_init(void) {
    const rsdp_t *rsdp = find_rsdp();
    if (!rsdp) {
        klogf(LOG_WARN, "acpi: no RSDP found, assuming 1 CPU\n");
        cpu_count = 1;
        cpu_apicids[0] = 0;
        return;
    }
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
