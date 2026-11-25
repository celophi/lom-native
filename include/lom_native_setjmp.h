#ifndef LOM_NATIVE_SETJMP_H
#define LOM_NATIVE_SETJMP_H

#ifdef _WIN32
typedef struct
{
    _Alignas(16) unsigned char saved[256];
} LomJumpBuffer;

/**
 * @brief The C runtime's _setjmp on Windows, with a buffer big enough for it
 *        and no frame to unwind to. Use LOM_SETJMP.
 * @param jump Where to save the place.
 * @param frame 0.
 * @return 0, then the value lom_native_runtime_longjmp passes.
 */
__attribute__((dllimport, returns_twice, nothrow)) int lom_native_runtime_setjmp(LomJumpBuffer* jump, void* frame) __asm__("_setjmp");

/**
 * @brief The C runtime's longjmp on Windows. Use LOM_LONGJMP.
 * @param jump The place lom_native_runtime_setjmp saved.
 * @param value What setjmp returns there; nonzero.
 */
__attribute__((dllimport, noreturn)) void lom_native_runtime_longjmp(LomJumpBuffer* jump, int value) __asm__("longjmp");

#define LOM_SETJMP(jump) lom_native_runtime_setjmp(&(jump), 0)
#define LOM_LONGJMP(jump, value) lom_native_runtime_longjmp(&(jump), (value))
#else
#include <setjmp.h>

typedef jmp_buf LomJumpBuffer;

#define LOM_SETJMP(jump) setjmp(jump)
#define LOM_LONGJMP(jump, value) longjmp((jump), (value))
#endif

#endif
