/**
 * @file elf.h
 * @brief 32-bit ELF header/section/program-header structures and the loader
 *        used by @ref start_proc.
 * @see https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
 */
#pragma once

#include <proc.h>
//XXX#include <fs/vfs.h>
#include <types.h>

// 0x7f followed by 'ELF' in ASCII. Little endian so in reverse.
#define ELF_MAGIC_NUMBER (0x464c457fUL)

#define EXE_MAX_SEGMENTS    (9)

#define ELF_X86     (0x3)
#define ELF_X86_64  (0x3E)
#define ELF_ARM     (0x28)

#define ELF_LITTLE_ENDIAN   (1)
#define ELF_32BIT           (1)

// Represents the 'program' type.
enum elf_pt_type {
	ELF_PT_NULL = 0,
	ELF_PT_LOAD = 1,
	ELF_PT_DYMAMIC = 2,
	ELF_PT_INTERP = 3,
	ELF_PT_NOTE = 4,
	ELF_PT_SHLIB = 5,
	ELF_PT_PHDR = 6,
	ELF_PT_TLS = 7,
};

// Represents the section header type.
enum elf_sh_type {
	ELF_SHT_NULL = 0,
	ELF_SHT_PROGBITS = 1,
	ELF_SHT_SYMTAB = 2,
	ELF_SHT_STRTAB = 3,
	ELF_SHT_RELA = 4,
	ELF_SHT_HASH = 5,
	ELF_SHT_DYNAMIC = 6,
	ELF_SHT_NOTE = 7,
	ELF_SHT_NOBITS = 8,
	ELF_SHT_REL = 9,
	ELF_SHT_SHLIB = 10,
	ELF_SHT_DYNSYM = 11,
	ELF_SHT_INIT_ARRAY = 14,
	ELF_SHT_FINI_ARRAY = 15,
	ELF_SHT_PREINIT_ARRAY = 16,
	ELF_SHT_GROUP = 17,
	ELF_SHT_SYMTAB_SHNDX = 18,
	ELF_SHT_NUM = 19,
};

// Directly maps to initial ELF header.
typedef struct elf_header {
    uint32_t magic;                    // Must = ELF_MAGIC_NUMBER. 
    uint8_t arch;                      // 1 = 32-bit, 2 = 64-bit.
    uint8_t machine;                   // 1 = little endian, 2 = big endian.
    uint8_t elf_version;               // = 1.
    uint8_t abi;                       // Target ABI (often set to 0 by default).
    uint8_t abi_ver;                   // ABI 'version'.
    uint8_t unused[7];
    uint16_t type;                     // Executable file type.
    uint16_t instruction_set;          // Target arch.
    uint32_t elf_version2;             // = 1.
    uint32_t entry;                    // Entry point address.
    uint32_t program_header;           // Offset to program headers (usually right after).
    uint32_t section_header;           // Offset to section header table.
    uint32_t flags;                    // Arch-specific flags.
    uint16_t header_size;              // Size of header.
    uint16_t entry_size_prog_header;   // Size of program header table entry.
    uint16_t entry_number_prog_header; // Number of entries in program header table.
    uint16_t entry_size_sect_header;   // Size of section header table entry.
    uint16_t entry_number_sect_header; // Number of entries in section header table.
    uint16_t index_sect_header_names;  // Index of section header table entry containing
			               // section names.
} __attribute__((__packed__)) elf_header_t;
_Static_assert(sizeof(struct elf_header) == 52, "elf_header must be 52 bytes");

// Directly maps to ELF section headers.
typedef struct elf_section_header {
    uint32_t s_name;                   // Offset in .shstrtab to name of section.
    uint32_t s_type;                   // Type of header.
    uint32_t s_flags;                  // Section attributes.
    uint32_t s_addr;                   // VA of section in memory (when loaded).
    uint32_t s_offset;                 // Offset of section in file (bytes).
    uint32_t s_size;                   // Size of section in file (bytes).
    uint32_t s_link;                   // Section index of an associated section.
    uint32_t s_info;                   // Extra information about section.
    uint32_t s_align;                  // Required alignment of section. Must be power of 2.
    uint32_t s_entsize;                // Size of each entry (bytes) if contains fixed size
			               // entries, otherwise 0.
} __attribute__((__packed__)) elf_section_header_t;
_Static_assert(sizeof(struct elf_section_header) == 40, "elf_section_header must be 40 bytes");

// Directly maps to ELF 'program' headers.
typedef struct elf_program_header {
    /*
     * 0 = NULL
     * 1 = load
     * 2 = dynamic
     * 3 = interp
     * 4 = notes
     */
    uint32_t p_type;      // Program header type.
    uint32_t p_offset;    // Offset of segment in file image.
    uint32_t p_vaddr;     // VA of segment in memory.
    uint32_t undefined;   // PA of segment in memory.
    uint32_t p_file_size; // Size of segment in file (bytes).
    uint32_t p_mem_size;  // Size of segment in memory (bytes).
    /*
     * 1 = executable
     * 2 = writable
     * 4 = readable
     */
    uint32_t p_flags;
    uint32_t align;      // 0, 1 = no alignment. Otherwise is a power of 2.
			 // p_vaddr = p_offset % align.
} __attribute__((__packed__)) elf_program_header_t;
_Static_assert(sizeof(struct elf_program_header) == 32, "elf_program_header must be 32 bytes");

/** @brief Sanity-check an ELF header (magic, 32-bit little-endian x86). */
int elf_validate(elf_header_t *eh);
/** @brief Load @p name, validate it, relocate its segments into @p pdir. */
int load_elf(char *name, thread_t *thread, page_dir_t *pdir);
/** @brief Read @p name into the staging area at @c MEMORY_LOAD_ADDRESS. */
int load_elf_file(char *name);
/** @brief Copy each PT_LOAD segment to its p_vaddr and record the image span. */
int load_elf_relocate(thread_t *thread, page_dir_t *pdir, elf_header_t *eh);

