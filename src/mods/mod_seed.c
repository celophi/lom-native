#include "lom_native_mod_seed.h"

static int s_wanted;

static int s_has_current;

static uint32_t s_current;

void lom_native_mod_seed_reset(void)
{
    s_wanted = 0;
    s_has_current = 0;
    s_current = 0;
}

void lom_native_mod_seed_set_wanted(int wanted)
{
    s_wanted = wanted != 0;
}

int lom_native_mod_seed_wanted(void)
{
    return s_wanted;
}

void lom_native_mod_seed_set_current(uint32_t seed)
{
    s_current = seed;
    s_has_current = 1;
}

int lom_native_mod_seed_current(uint32_t* seed)
{
    if (!s_has_current)
    {
        return 0;
    }

    *seed = s_current;
    return 1;
}
