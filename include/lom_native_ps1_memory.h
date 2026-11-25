#ifndef LOM_NATIVE_PS1_MEMORY_H
#define LOM_NATIVE_PS1_MEMORY_H

#include <stdint.h>

#define LOM_PS1_RAM_BASE ((uintptr_t)0x80000000u)
#define LOM_PS1_RAM_ALIAS_BASE ((uintptr_t)0x10000000u)
#define LOM_PS1_RAM_SIZE ((uintptr_t)0x00200000u)
#define LOM_PS1_SCRATCHPAD_BASE ((uintptr_t)0x1F800000u)
#define LOM_PS1_SCRATCHPAD_SIZE ((uintptr_t)0x400u)

/**
 * @brief Check PS1 RAM can be used (src/core/native_memory.c).
 * @return Zero on success, nonzero if the program's data sits too high on a
 *        32-bit host.
 */
int lom_native_map_ps1_ram(void);

/**
 * @brief Log once per call site that the game took a path where the PS1 result
 *        depends on an uninitialized local, and the port used a fixed value
 *        instead.
 * @param site Function and condition, for the log.
 * @note The log line starts with "[lom-native] undefined path", so it's easy to
 *        grep for when checking whether real game data ever hits one.
 */
void lom_native_note_undefined_path(const char* site);

#define LOM_NATIVE_UNDEFINED_PATH(site)                                                                                                                        \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        static int lom_native_undefined_noted_;                                                                                                                \
        if (!lom_native_undefined_noted_)                                                                                                                      \
        {                                                                                                                                                      \
            lom_native_undefined_noted_ = 1;                                                                                                                   \
            lom_native_note_undefined_path(site);                                                                                                              \
        }                                                                                                                                                      \
    } while (0)

extern unsigned char lom_native_ram[];

/**
 * @brief Read through a pointer the way the PS1 did: a NULL pointer reads PS1
 *        RAM at address 0.
 * @param pointer The pointer.
 * @return @p pointer, or the start of PS1 RAM for NULL.
 */
static inline void* lom_native_ps1_or_ram0(const void* pointer)
{
    return pointer != 0 ? (void*)pointer : (void*)lom_native_ram;
}

/**
 * @brief Read through a pointer the way the PS1 did when it's a small number:
 *        an address in the first 2 MB reads PS1 RAM there, through the low
 *        mirror.
 * @param pointer The pointer.
 * @return Where in PS1 RAM it points, or @p pointer when it's a real host
 *        pointer.
 */
static inline void* lom_native_ps1_low_ram(const void* pointer)
{
    if ((uintptr_t)pointer < LOM_PS1_RAM_SIZE)
    {
        return (void*)(lom_native_ram + (uintptr_t)pointer * 4);
    }

    return (void*)pointer;
}

void* lom_unit_host_address(unsigned int address, const char* where);
#define LOM_PS1_ADDRESS(address) lom_unit_host_address(0x80000000u | ((address) & 0x1FFFFFu), __func__)

#define LOM_PS1_RAM0 ((uintptr_t)lom_native_ram)

#define LOM_PS1_OR_RAM0(pointer) ((__typeof__(*(pointer))*)lom_native_ps1_or_ram0(pointer))
#define LOM_PS1_LOW_RAM(pointer) ((__typeof__(*(pointer))*)lom_native_ps1_low_ram(pointer))

/** The host address of a PS1 address in a fixed region, or NULL if it isn't in one. */
void* lom_native_fixed_address(unsigned int address);

/** The PS1 address of a host address in a fixed region, or 0 if it isn't in one. */
unsigned int lom_native_fixed_ps1_address(const void* host);

#endif
