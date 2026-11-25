#include "lom_native_random.h"
#include <stdint.h>

static uint32_t s_state = 1;

int lom_native_rand(void)
{
    s_state = s_state * UINT32_C(0x41C64E6D) + UINT32_C(0x3039);
    return (int)((s_state >> 16) & 0x7FFF);
}

void lom_native_srand(unsigned int seed)
{
    s_state = (uint32_t)seed;
}
