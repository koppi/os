#pragma once

#include <stdint.h>

#define NULL 0

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
//typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef signed char int8_t;
typedef signed short int16_t;
//typedef signed int int32_t;
typedef signed long long int64_t;

typedef unsigned int size_t;

typedef long int ptrdiff_t;
typedef __UINTPTR_TYPE__ uintptr_t;

typedef int pid_t;

typedef struct regs16 {
    uint16_t di;
    uint16_t si;
    uint16_t bp;
    uint16_t sp;
    uint16_t bx;
    uint16_t dx;
    uint16_t cx;
    uint16_t ax;
    uint16_t gs;
    uint16_t fs;
    uint16_t es;
    uint16_t ds;
    uint16_t eflags;
} __attribute__ ((packed)) regs16_t;

typedef __builtin_va_list va_list;
#define va_start(ap,last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap,type) __builtin_va_arg(ap,type)

