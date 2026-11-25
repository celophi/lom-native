#include "overlays/field/internal/field_script.h"
#include "overlays/field/internal/field_calls.h"
#include "overlays/field/internal/field_records.h"
#include "main/audio/akao.h"
#include "main/field_entry.h"
#include "route_record.h"
#include "route_record_internal.h"
#include <stdint.h>
#include <stdio.h>

enum
{
    PAIR_OP_BASE = 0x40,
    PAIR_OP_COUNT = 0x20,
    EXT_OP_BASE = 0x80,
    EXT_OP_COUNT = 0x10,
};

enum
{
    AKAO_SEQUENCE_MAGIC = 0x4F414B41u,
    SCENE_MASK = 0x7FFF,
};

enum
{
    COVER_SET_BITS = 17,
    COVER_SET_SIZE = 1 << COVER_SET_BITS,
    COVER_KIND_SHIFT = 56,
    COVER_A_SHIFT = 32,
    COVER_B_SHIFT = 16,
    COVER_VALUE_MASK = 0xFFFF,
};

static const unsigned long long COVER_HASH_MULTIPLIER = 0x9E3779B97F4A7C15ull;

extern void __real_lom_native_field_script_handler(void* handler);
extern void (*g_field_script_op_table[])();
extern void (*g_field_script_pair_op_table[])();
#define g_field_scene_id (g_field_entry_scene.word)

static unsigned long long s_cover[COVER_SET_SIZE];

static unsigned s_cover_count;

static unsigned s_cover_dropped;

/**
 * @brief Pack a coverage item into its key in the coverage set.
 * @param kind What it is.
 * @param a First value; only its low 16 bits are kept.
 * @param b Second value; only its low 16 bits are kept.
 * @param c Third value; only its low 16 bits are kept.
 * @return The key, never 0.
 */
static unsigned long long cover_key(RouteCoverKind kind, unsigned a, unsigned b, unsigned c)
{
    unsigned long long key = (unsigned long long)kind << COVER_KIND_SHIFT;

    key |= (unsigned long long)(a & COVER_VALUE_MASK) << COVER_A_SHIFT;
    key |= (unsigned long long)(b & COVER_VALUE_MASK) << COVER_B_SHIFT;
    key |= c & COVER_VALUE_MASK;
    return key + 1;
}

void route_record_cover(RouteCoverKind kind, unsigned a, unsigned b, unsigned c)
{
    unsigned long long key = cover_key(kind, a, b, c);
    unsigned slot = (unsigned)((key * COVER_HASH_MULTIPLIER) >> (64 - COVER_SET_BITS)) & (COVER_SET_SIZE - 1);

    while (s_cover[slot] != 0)
    {
        if (s_cover[slot] == key)
        {
            return;
        }

        slot = (slot + 1) & (COVER_SET_SIZE - 1);
    }

    if (s_cover_count >= COVER_SET_SIZE / 2)
    {
        s_cover_dropped++;
        return;
    }

    s_cover[slot] = key;
    s_cover_count++;
}

unsigned route_record_cover_dropped(void)
{
    return s_cover_dropped;
}

void route_record_write_cover(FILE* file)
{
    for (unsigned i = 0; i < COVER_SET_SIZE; i++)
    {
        if (s_cover[i] == 0)
        {
            continue;
        }

        unsigned long long item = s_cover[i] - 1;
        unsigned kind = (unsigned)(item >> COVER_KIND_SHIFT);
        unsigned a = (unsigned)(item >> COVER_A_SHIFT) & COVER_VALUE_MASK;
        unsigned b = (unsigned)(item >> COVER_B_SHIFT) & COVER_VALUE_MASK;
        unsigned c = (unsigned)item & COVER_VALUE_MASK;

        switch (kind)
        {
        case ROUTE_COVER_INSTRUCTION:
            fprintf(file, "instruction %u %u\n", a, b | ((c & 0xFF) << 16));
            break;
        case ROUTE_COVER_INSTRUCTION_END:
            fprintf(file, "instruction_end %u %u %u\n", a, b | ((c & 0xFF) << 16), c >> 8);
            break;
        case ROUTE_COVER_SCENE:
            fprintf(file, "scene %u\n", a);
            break;
        case ROUTE_COVER_EXIT:
            fprintf(file, "exit %u %u %u\n", a, b, c);
            break;
        case ROUTE_COVER_SONG:
            fprintf(file, "song %u\n", a);
            break;
        case ROUTE_COVER_MONSTER:
            fprintf(file, "monster %u %u\n", a, b);
            break;
        default:
            break;
        }
    }
}

/**
 * @brief Record which FIELD script instruction a handler runs, for the route's
 *        coverage.
 * @param handler The handler's address.
 */
static void cover_handler(uintptr_t handler)
{
    FieldScriptState* context = g_field_script;

    if (route_record_recording() && context != NULL && (context->status.word & FIELD_SCRIPT_RUNNING))
    {
        u8* pc = FIELD_SCRIPT_RECORD_STATE(context->depth)->pc;
        unsigned offset = (unsigned)(pc - (u8*)g_field_event_scripts);
        unsigned scene = (unsigned)g_field_scene_id & SCENE_MASK;

        if (*pc < PAIR_OP_BASE && (uintptr_t)g_field_script_op_table[*pc] == handler)
        {
            route_record_cover(ROUTE_COVER_INSTRUCTION, scene, offset, offset >> 16);
            return;
        }

        for (unsigned i = 0; i < PAIR_OP_COUNT; i++)
        {
            if ((uintptr_t)g_field_script_pair_op_table[i] == handler)
            {
                route_record_cover(ROUTE_COVER_INSTRUCTION_END, scene, offset, (offset >> 16) | ((PAIR_OP_BASE + i) << 8));
                return;
            }
        }

        for (unsigned i = 0; i < EXT_OP_COUNT; i++)
        {
            if ((uintptr_t)g_field_script_ext_op_table[i] == handler)
            {
                route_record_cover(ROUTE_COVER_INSTRUCTION_END, scene, offset, (offset >> 16) | ((EXT_OP_BASE + i) << 8));
                return;
            }
        }
    }
}

/**
 * @brief Record the script instruction, then run the real handler hook.
 * @param handler The handler's address.
 */
void __wrap_lom_native_field_script_handler(void* handler)
{
    cover_handler((uintptr_t)handler);
    __real_lom_native_field_script_handler(handler);
}

extern s32 __real_akao_play_song(AkaoHeader* sequence);

extern s32 __real_akao_switch_song(void* sequence, s32 volume);

/**
 * @brief Record the song before playing it.
 * @param sequence The song's AKAO sequence.
 * @return The real function's result.
 */
s32 __wrap_akao_play_song(AkaoHeader* sequence)
{
    if (sequence)
    {
        route_record_cover(ROUTE_COVER_SONG, sequence->id, 0, 0);
    }

    return __real_akao_play_song(sequence);
}

/**
 * @brief Record the song before switching to it.
 * @param sequence The song's AKAO sequence.
 * @param volume Volume for the new song.
 * @return The real function's result.
 */
s32 __wrap_akao_switch_song(void* sequence, s32 volume)
{
    if (sequence)
    {
        route_record_cover(ROUTE_COVER_SONG, ((AkaoHeader*)sequence)->id, 0, 0);
    }

    return __real_akao_switch_song(sequence, volume);
}

extern s32 __real_lom_native_akao_start_song_channels(void* sequence, s32 channel_mask, s32 unused);

/**
 * @brief Record a song started one section of a container at a time (FIELD's
 *        field_play_song_section, for the fixed song container's songs), then
 *        start it.
 * @param sequence The section, an AKAO sequence.
 * @param channel_mask Channels to start.
 * @param unused Unused.
 * @return The real function's result.
 */
s32 __wrap_lom_native_akao_start_song_channels(void* sequence, s32 channel_mask, s32 unused)
{
    if (sequence && ((AkaoHeader*)sequence)->magic == AKAO_SEQUENCE_MAGIC)
    {
        route_record_cover(ROUTE_COVER_SONG, ((AkaoHeader*)sequence)->id, 0, 0);
    }

    return __real_lom_native_akao_start_song_channels(sequence, channel_mask, unused);
}
