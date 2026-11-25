#ifndef _COMMON_H
#define _COMMON_H

#include "include_asm.h"
#include "common/memory_map.h"

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
#ifdef _WIN64
typedef unsigned long long u_long;
#else
typedef unsigned long u_long;
#endif

typedef unsigned char undefined;
typedef unsigned char undefined1;
typedef unsigned short undefined2;
typedef unsigned int undefined4;

typedef int s32;
typedef unsigned int u32;
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;

#if defined(__mips__) && defined(__GNUC__) && __GNUC__ < 3
typedef u32 uintptr_t;
typedef s32 intptr_t;
#else
#include <stdint.h>
#endif

#define BYTES_PER_WORD 4
#define BYTES_PER_WORD_SHIFT 2
#define BYTES_PER_WORD_MASK (BYTES_PER_WORD - 1)

#define TRUE 1
#define FALSE 0
#define NULL ((void*)0)

#define MAX_SHORT_VALUE 32767

#define PACK_U16_PAIR(low, high) ((u32)(low) + ((u32)(high) << 16))

#define ALIGN64(x) (((x) + 0x3F) & 0xFFC0)

#define MIN(a, b) ((a) > (b) ? (b) : (a))

#define ALIGN4(x) (((uintptr_t)(x) + 3) & ~3)

#define ALIGN2(x) (((uintptr_t)(x) + 1) & ~1)

#define OFFSETOF(type, member) ((u32)((u8*)&((type*)0)->member - (u8*)0))

#endif
