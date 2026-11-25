#include "lom_native_draw_labels.h"
#include "lom_native_env.h"
#include <libgpu.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    CAPACITY = 4096
};

typedef struct
{
    uintptr_t start, end;
    LomDrawLabel label;
} Range;

typedef struct
{
    Range ranges[CAPACITY];
    int count;
    int ordered;
} List;

typedef struct
{
    List things, parts;
} Frame;

static Frame s_frames[2];
static int s_current;
static int s_full_reported;

/**
 * @brief Empty a list of labeled ranges.
 * @param list The list.
 */
static void clear_list(List* list)
{
    list->count = 0;
    list->ordered = 1;
}

void lom_native_draw_labels_next_frame(void)
{
    s_current ^= 1;
    clear_list(&s_frames[s_current].things);
    clear_list(&s_frames[s_current].parts);
}

void lom_native_draw_labels_clear(void)
{
    for (int i = 0; i < 2; ++i)
    {
        clear_list(&s_frames[i].things);
        clear_list(&s_frames[i].parts);
    }
}

/**
 * @brief Label a range of packet memory. A label past the list's room is
 *        dropped, reported once.
 * @param list The list.
 * @param label The label.
 * @param start Where the range starts.
 * @param end Where it ends.
 */
static void add(List* list, LomDrawLabel label, const void* start, const void* end)
{
    if ((uintptr_t)end <= (uintptr_t)start)
    {
        return;
    }

    if (list->count == CAPACITY)
    {
        if (!s_full_reported)
        {
            s_full_reported = 1;
            fprintf(stderr, "[lom-native] graphics labels: more than %d in a frame; the rest go unlabelled\n", CAPACITY);
        }

        return;
    }

    if (list->count == 0)
    {
        list->ordered = 1;
    }
    else if ((uintptr_t)start < list->ranges[list->count - 1].end)
    {
        list->ordered = 0;
    }

    list->ranges[list->count].start = (uintptr_t)start;
    list->ranges[list->count].end = (uintptr_t)end;
    list->ranges[list->count].label = label;
    ++list->count;
}

void lom_native_draw_label(int kind, int id, int detail, const void* start, const void* end)
{
    add(&s_frames[s_current].things, (LomDrawLabel){kind, id, detail, 0}, start, end);
}

void lom_native_draw_label_part(const char* name, int id, const void* start, const void* end)
{
    add(&s_frames[s_current].parts, (LomDrawLabel){LOM_LABEL_PART, id, 0, name}, start, end);
}

/**
 * @brief Find the label of the range an address is in: by binary search while
 *        the ranges were added in order, otherwise one by one.
 * @param list The list.
 * @param at The address.
 * @return The label, or NULL.
 */
static const LomDrawLabel* find_in(const List* list, uintptr_t at)
{
    if (list->ordered)
    {
        int low = 0, high = list->count;

        while (low < high)
        {
            const int middle = (low + high) / 2;

            if (list->ranges[middle].end <= at)
            {
                low = middle + 1;
            }
            else
            {
                high = middle;
            }
        }

        return low < list->count && list->ranges[low].start <= at ? &list->ranges[low].label : 0;
    }

    for (int i = 0; i < list->count; ++i)
    {
        if (at >= list->ranges[i].start && at < list->ranges[i].end)
        {
            return &list->ranges[i].label;
        }
    }

    return 0;
}

const LomDrawLabel* lom_native_draw_label_find(const void* packet)
{
    const uintptr_t at = (uintptr_t)packet;
    const Frame* now = &s_frames[s_current];
    const Frame* before = &s_frames[s_current ^ 1];
    const LomDrawLabel* label = find_in(&now->things, at);
    label = label ? label : find_in(&before->things, at);
    label = label ? label : find_in(&now->parts, at);
    return label ? label : find_in(&before->parts, at);
}

enum
{
    CHECK_SCREENS = 16
};

typedef struct
{
    const char* screen;
    unsigned long long packets, unlabelled;
    unsigned first_code;
} Coverage;

static Coverage s_coverage[CHECK_SCREENS];
static int s_check = -1;

void lom_native_draw_labels_check(const char* screen, const void* ordering_table)
{
    if (s_check < 0)
    {
        s_check = lom_native_env_on(LOM_ENV_DRAW_LABEL_CHECK);
    }

    if (!s_check || !ordering_table)
    {
        return;
    }

    Coverage* coverage = 0;

    for (int i = 0; i < CHECK_SCREENS && !coverage; ++i)
    {
        if (!s_coverage[i].screen || strcmp(s_coverage[i].screen, screen) == 0)
        {
            coverage = &s_coverage[i];
            coverage->screen = screen;
        }
    }

    if (!coverage)
    {
        return;
    }

    P_TAG* packet = (P_TAG*)ordering_table;

    for (int steps = 0; steps < 1 << 22; ++steps)
    {
        if (getlen(packet) > 0)
        {
            ++coverage->packets;

            if (!lom_native_draw_label_find(packet))
            {
                if (coverage->unlabelled++ == 0)
                {
                    coverage->first_code = packet->code;
                }
            }
        }

        if (isendprim(packet))
        {
            break;
        }

        packet = nextPrim(packet);
    }
}

void lom_native_draw_labels_report(void)
{
    for (int i = 0; i < CHECK_SCREENS && s_coverage[i].screen; ++i)
    {
        const Coverage* coverage = &s_coverage[i];
        printf("[lom-native] graphics labels: %s %llu packets, %llu without a label", coverage->screen, coverage->packets, coverage->unlabelled);

        if (coverage->unlabelled)
        {
            printf(" (the first was command %02X)", coverage->first_code);
        }

        printf("\n");
    }
}
