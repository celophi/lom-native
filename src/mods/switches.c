#include "lom_native_switches.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    const char* name;
    int record_number;
} SwitchInfo;

static const SwitchInfo s_switches[LOM_SWITCH_COUNT] = {
    [LOM_SWITCH_ALWAYS_RUN] = {"always_run", 0},
    [LOM_SWITCH_SKIP_BATTLE_RESULTS] = {"skip_battle_results", 1},
    [LOM_SWITCH_NO_NATURAL_HEALING] = {"no_natural_healing", 2},
    [LOM_SWITCH_SKIP_MOVIES] = {"skip_movies", 3},
    [LOM_SWITCH_SKIP_BATTLES] = {"skip_battles", 4},
    [LOM_SWITCH_JAPANESE_BALANCE] = {"japanese_balance", 5},
};

static unsigned char s_setting[LOM_SWITCH_COUNT];

static unsigned char s_mod_value[LOM_SWITCH_COUNT];

static const char* s_mod_owner[LOM_SWITCH_COUNT];

static unsigned char s_held[LOM_SWITCH_COUNT];

static int s_holding;

/**
 * @brief Check a switch id is one the registry has.
 * @param id The id.
 * @return 1 when it's known, otherwise 0.
 */
static int known(LomSwitch id)
{
    return (unsigned)id < LOM_SWITCH_COUNT;
}

int lom_native_switch(LomSwitch id)
{
    if (!known(id))
    {
        return 0;
    }

    return s_holding ? s_held[id] : lom_native_switch_wanted(id);
}

int lom_native_switch_wanted(LomSwitch id)
{
    if (!known(id))
    {
        return 0;
    }

    return s_mod_owner[id] ? s_mod_value[id] : s_setting[id];
}

int lom_native_switch_setting(LomSwitch id)
{
    return known(id) ? s_setting[id] : 0;
}

void lom_native_switch_set_setting(LomSwitch id, int on)
{
    if (known(id))
    {
        s_setting[id] = on != 0;
    }
}

void lom_native_switch_set_by_mod(LomSwitch id, int on, const char* mod_id)
{
    if (known(id) && mod_id)
    {
        s_mod_value[id] = on != 0;
        s_mod_owner[id] = mod_id;
    }
}

const char* lom_native_switch_mod(LomSwitch id)
{
    return known(id) ? s_mod_owner[id] : NULL;
}

void lom_native_switch_clear_mods(void)
{
    memset(s_mod_owner, 0, sizeof(s_mod_owner));
    memset(s_mod_value, 0, sizeof(s_mod_value));
}

void lom_native_switch_hold(LomSwitch id, int on)
{
    if (!known(id))
    {
        return;
    }

    if (!s_holding)
    {
        for (int i = 0; i < LOM_SWITCH_COUNT; i++)
        {
            s_held[i] = (unsigned char)lom_native_switch_wanted((LomSwitch)i);
        }

        s_holding = 1;
    }

    s_held[id] = on != 0;
}

void lom_native_switch_release(void)
{
    s_holding = 0;
}

const char* lom_native_switch_name(LomSwitch id)
{
    return known(id) ? s_switches[id].name : NULL;
}

int lom_native_switch_find(const char* name)
{
    if (!name)
    {
        return -1;
    }

    for (int i = 0; i < LOM_SWITCH_COUNT; i++)
    {
        if (strcmp(s_switches[i].name, name) == 0)
        {
            return i;
        }
    }

    return -1;
}

int lom_native_switch_record_number(LomSwitch id)
{
    return known(id) ? s_switches[id].record_number : -1;
}

int lom_native_switch_from_record_number(int number)
{
    for (int i = 0; i < LOM_SWITCH_COUNT; i++)
    {
        if (s_switches[i].record_number == number)
        {
            return i;
        }
    }

    return -1;
}
