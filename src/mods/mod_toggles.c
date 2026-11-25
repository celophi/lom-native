#include "lom_native_mod_toggles.h"

#include <stddef.h>
#include <string.h>

#define MAX_TOGGLES 64
#define ID_BYTES 65
#define NAME_BYTES 128
#define DESCRIPTION_BYTES 512

typedef struct
{
    char id[ID_BYTES];
    char name[NAME_BYTES];
    char description[DESCRIPTION_BYTES];
    unsigned char wanted;
    unsigned char held;
} Toggle;

static Toggle s_toggles[MAX_TOGGLES];

static int s_count;

static int s_holding;

static void (*s_listener)(void);

/**
 * @brief Copy text into a fixed buffer, cutting it short when it doesn't fit.
 * @param to The buffer.
 * @param size Its size.
 * @param from The text, or NULL for none.
 */
static void copy_text(char* to, size_t size, const char* from)
{
    size_t length = from ? strlen(from) : 0;

    if (length >= size)
    {
        length = size - 1;
    }

    if (length)
    {
        memcpy(to, from, length);
    }

    to[length] = '\0';
}

/**
 * @brief Check an index is one of the toggles.
 * @param index The index.
 * @return 1 when it is.
 */
static int known(int index)
{
    return index >= 0 && index < s_count;
}

void lom_native_mod_toggles_reset(void)
{
    s_count = 0;
    s_holding = 0;
    s_listener = NULL;
}

int lom_native_mod_toggles_add(const char* id, const char* name, const char* description)
{
    if (s_count >= MAX_TOGGLES)
    {
        return -1;
    }

    Toggle* toggle = &s_toggles[s_count];
    copy_text(toggle->id, sizeof(toggle->id), id);
    copy_text(toggle->name, sizeof(toggle->name), name);
    copy_text(toggle->description, sizeof(toggle->description), description);
    toggle->wanted = 1;
    toggle->held = 1;
    return s_count++;
}

void lom_native_mod_toggles_set_listener(void (*listener)(void))
{
    s_listener = listener;
}

int lom_native_mod_toggles_count(void)
{
    return s_count;
}

const char* lom_native_mod_toggles_id(int index)
{
    return known(index) ? s_toggles[index].id : NULL;
}

const char* lom_native_mod_toggles_name(int index)
{
    return known(index) ? s_toggles[index].name : NULL;
}

const char* lom_native_mod_toggles_description(int index)
{
    return known(index) ? s_toggles[index].description : NULL;
}

int lom_native_mod_toggles_wanted(int index)
{
    return known(index) ? s_toggles[index].wanted : 0;
}

void lom_native_mod_toggles_set(int index, int on)
{
    if (!known(index) || s_toggles[index].wanted == (on != 0))
    {
        return;
    }

    s_toggles[index].wanted = on != 0;

    if (s_listener)
    {
        s_listener();
    }
}

int lom_native_mod_toggles_on(int index)
{
    if (!known(index))
    {
        return 0;
    }

    return s_holding ? s_toggles[index].held : s_toggles[index].wanted;
}

void lom_native_mod_toggles_hold(int index, int on)
{
    if (!known(index))
    {
        return;
    }

    if (!s_holding)
    {
        for (int i = 0; i < s_count; i++)
        {
            s_toggles[i].held = s_toggles[i].wanted;
        }

        s_holding = 1;
    }

    s_toggles[index].held = on != 0;
}

void lom_native_mod_toggles_release(void)
{
    s_holding = 0;
}
