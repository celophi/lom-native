#include "common/memory_map.h"
#include "lom_native_ps1_memory.h"

#include <stdint.h>
#include <stdio.h>

#define PS1_RAM_SIZE 0x200000
#define PS1_SCRATCHPAD_SIZE 0x400

_Alignas(16) unsigned char lom_native_ram[PS1_RAM_SIZE * MEMORY_MAP_SCALE];

_Alignas(16) unsigned char lom_native_scratchpad[PS1_SCRATCHPAD_SIZE * MEMORY_MAP_SCALE];

_Alignas(16) unsigned char lom_native_spu_registers[0x200];

void* lom_native_fixed_address(unsigned int address)
{
    if (address >= 0x1F801C00u && address < 0x1F801E00u)
    {
        return lom_native_spu_registers + (address - 0x1F801C00u);
    }
    if (address >= 0x1F800000u && address < 0x1F800000u + PS1_SCRATCHPAD_SIZE)
    {
        return (void*)MEMORY_MAP_SCRATCHPAD(address);
    }
    if ((address & 0xFFE00000u) == 0x80000000u || (address & 0xFFE00000u) == 0)
    {
        return (void*)MEMORY_MAP_RAM(address);
    }
    return NULL;
}

unsigned int lom_native_fixed_ps1_address(const void* host)
{
    const unsigned char* bytes = host;
    if (bytes >= lom_native_spu_registers && bytes < lom_native_spu_registers + sizeof(lom_native_spu_registers))
    {
        return 0x1F801C00u + (unsigned int)(bytes - lom_native_spu_registers);
    }
    if (bytes >= lom_native_scratchpad && bytes < lom_native_scratchpad + sizeof(lom_native_scratchpad))
    {
        return 0x1F800000u + (unsigned int)(bytes - lom_native_scratchpad) / MEMORY_MAP_SCALE;
    }
    if (bytes >= lom_native_ram && bytes < lom_native_ram + sizeof(lom_native_ram))
    {
        return 0x80000000u + (unsigned int)(bytes - lom_native_ram) / MEMORY_MAP_SCALE;
    }
    return 0;
}

#if UINTPTR_MAX == UINT32_MAX && defined(__ELF__)
extern char _end[];
#endif

int lom_native_map_ps1_ram(void)
{
#if UINTPTR_MAX == UINT32_MAX
#if defined(__ELF__)
    uintptr_t end = (uintptr_t)_end;
#else
    uintptr_t end = (uintptr_t)(lom_native_ram + sizeof(lom_native_ram));
#endif
    if (end > 0x80000000u)
    {
        fprintf(stderr, "lom: the program's data ends at %#lx, above 2 GiB; FIELD's frame marks need it below\n", (unsigned long)end);
        return -1;
    }
#endif
    return 0;
}

void lom_native_note_undefined_path(const char* site)
{
    fprintf(stderr, "[lom-native] undefined path: %s; using a defined substitute\n", site);
}
