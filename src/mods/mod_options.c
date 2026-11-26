#include "lom_native_mod_options.h"

#include <stddef.h>
#include <string.h>

#define MAX_OPTIONS 64
#define MAX_CHOICES 16
#define MAX_PREFERRED 256
#define ID_BYTES 65
#define NAME_BYTES 128
#define DESCRIPTION_BYTES 512

typedef struct
{
    char id[ID_BYTES];
    char name[NAME_BYTES];
    char description[DESCRIPTION_BYTES];
} Choice;

typedef struct
{
    char mod[ID_BYTES];
    char id[ID_BYTES];
    char name[NAME_BYTES];
    char description[DESCRIPTION_BYTES];
    Choice choices[MAX_CHOICES];
    int choice_count;
    int chosen;
} Option;

typedef struct
{
    char mod[ID_BYTES];
    char option[ID_BYTES];
    char choice[ID_BYTES];
} Preferred;

static Option s_options[MAX_OPTIONS];

static int s_count;

static Preferred s_preferred[MAX_PREFERRED];

static int s_preferred_count;

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
 * @brief Find an option by index.
 * @param index The index.
 * @return The option, or NULL when the index is out of range.
 */
static Option* option_at(int index)
{
    return index >= 0 && index < s_count ? &s_options[index] : NULL;
}

/**
 * @brief Find a choice by index.
 * @param index The option.
 * @param choice The choice.
 * @return The choice, or NULL when either is out of range.
 */
static const Choice* choice_at(int index, int choice)
{
    const Option* option = option_at(index);
    return option && choice >= 0 && choice < option->choice_count ? &option->choices[choice] : NULL;
}

/**
 * @brief Find the choice the player prefers for an option.
 * @param option The option.
 * @return The preferred choice's id, or NULL when they haven't chosen one.
 */
static const char* preferred_for(const Option* option)
{
    for (int i = 0; i < s_preferred_count; i++)
    {
        if (strcmp(s_preferred[i].mod, option->mod) == 0 && strcmp(s_preferred[i].option, option->id) == 0)
        {
            return s_preferred[i].choice;
        }
    }

    return NULL;
}

void lom_native_mod_options_reset(void)
{
    s_count = 0;
    s_listener = NULL;
}

int lom_native_mod_options_add(const char* mod, const char* id, const char* name, const char* description, int default_choice)
{
    if (s_count >= MAX_OPTIONS)
    {
        return -1;
    }

    Option* option = &s_options[s_count];
    copy_text(option->mod, sizeof(option->mod), mod);
    copy_text(option->id, sizeof(option->id), id);
    copy_text(option->name, sizeof(option->name), name);
    copy_text(option->description, sizeof(option->description), description);
    option->choice_count = 0;
    option->chosen = default_choice;
    return s_count++;
}

void lom_native_mod_options_add_choice(int index, const char* id, const char* name, const char* description)
{
    Option* option = option_at(index);

    if (!option || option->choice_count >= MAX_CHOICES)
    {
        return;
    }

    Choice* choice = &option->choices[option->choice_count];
    copy_text(choice->id, sizeof(choice->id), id);
    copy_text(choice->name, sizeof(choice->name), name);
    copy_text(choice->description, sizeof(choice->description), description);
    const char* preferred = preferred_for(option);

    if (preferred && strcmp(preferred, choice->id) == 0)
    {
        option->chosen = option->choice_count;
    }

    option->choice_count++;
}

void lom_native_mod_options_set_listener(void (*listener)(void))
{
    s_listener = listener;
}

int lom_native_mod_options_count(void)
{
    return s_count;
}

const char* lom_native_mod_options_mod(int index)
{
    const Option* option = option_at(index);
    return option ? option->mod : NULL;
}

const char* lom_native_mod_options_id(int index)
{
    const Option* option = option_at(index);
    return option ? option->id : NULL;
}

const char* lom_native_mod_options_name(int index)
{
    const Option* option = option_at(index);
    return option ? option->name : NULL;
}

const char* lom_native_mod_options_description(int index)
{
    const Option* option = option_at(index);
    return option ? option->description : NULL;
}

int lom_native_mod_options_choice_count(int index)
{
    const Option* option = option_at(index);
    return option ? option->choice_count : 0;
}

const char* lom_native_mod_options_choice_id(int index, int choice)
{
    const Choice* found = choice_at(index, choice);
    return found ? found->id : NULL;
}

const char* lom_native_mod_options_choice_name(int index, int choice)
{
    const Choice* found = choice_at(index, choice);
    return found ? found->name : NULL;
}

const char* lom_native_mod_options_choice_description(int index, int choice)
{
    const Choice* found = choice_at(index, choice);
    return found ? found->description : NULL;
}

int lom_native_mod_options_chosen(int index)
{
    const Option* option = option_at(index);

    if (!option)
    {
        return -1;
    }

    return option->chosen < option->choice_count ? option->chosen : 0;
}

void lom_native_mod_options_choose(int index, int choice)
{
    Option* option = option_at(index);

    if (!option || choice < 0 || choice >= option->choice_count)
    {
        return;
    }

    option->chosen = choice;

    if (s_listener)
    {
        s_listener();
    }
}

void lom_native_mod_options_prefer(const char* mod, const char* option, const char* choice)
{
    if (!mod || !option || !choice)
    {
        return;
    }

    Preferred* entry = NULL;

    for (int i = 0; i < s_preferred_count && !entry; i++)
    {
        if (strcmp(s_preferred[i].mod, mod) == 0 && strcmp(s_preferred[i].option, option) == 0)
        {
            entry = &s_preferred[i];
        }
    }

    if (!entry && s_preferred_count < MAX_PREFERRED)
    {
        entry = &s_preferred[s_preferred_count++];
        copy_text(entry->mod, sizeof(entry->mod), mod);
        copy_text(entry->option, sizeof(entry->option), option);
    }

    if (!entry)
    {
        return;
    }

    copy_text(entry->choice, sizeof(entry->choice), choice);

    for (int i = 0; i < s_count; i++)
    {
        Option* loaded = &s_options[i];

        if (strcmp(loaded->mod, mod) != 0 || strcmp(loaded->id, option) != 0)
        {
            continue;
        }

        for (int j = 0; j < loaded->choice_count; j++)
        {
            if (strcmp(loaded->choices[j].id, choice) == 0)
            {
                lom_native_mod_options_choose(i, j);
            }
        }
    }
}
