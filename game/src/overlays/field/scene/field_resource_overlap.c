#if defined(LOM_NATIVE_POINTERS)

#include "overlays/field/field_actor_records.h"
#include "overlays/field/internal/field_effect_render_state.h"
#include "overlays/field/internal/field_effect_types.h"

#include <string.h>

#define PS1_ENTRY_SIZE 20
#define PS1_ENTRY_PLAIN_OFFSET 8
#define PS1_ENTRY_PLAIN_SIZE 12
#define FIRST_OVERLAPPING_ENTRY 9
#define LAST_ENTRY 12

#define BOUNDS_OFFSET 4
#define COUNT_OFFSET 0x44
#define GAP_OFFSET 0x48
#define EFFECT_OFFSET 0x4C
#define OVERLAP_END 0x50

static unsigned char s_gap[4];

/**
 * @brief The native storage of the PS1 byte at an offset from the cursor, or NULL for the cursor's own bytes.
 * @param offset Offset from 0x800FF60C.
 */
static unsigned char* overlap_byte(unsigned offset)
{
    if (offset < BOUNDS_OFFSET)
    {
        return NULL;
    }

    if (offset < COUNT_OFFSET)
    {
        return (unsigned char*)g_field_group_bounds + (offset - BOUNDS_OFFSET);
    }

    if (offset < GAP_OFFSET)
    {
        return (unsigned char*)&g_field_group_bounds_count + (offset - COUNT_OFFSET);
    }

    if (offset < EFFECT_OFFSET)
    {
        return s_gap + (offset - GAP_OFFSET);
    }

    return (unsigned char*)&g_field_effect_records[0].x + (offset - EFFECT_OFFSET);
}

/**
 * @brief The plain fields of a native entry, in PS1 order, as bytes.
 * @param entry Native entry.
 * @param bytes Twelve bytes: multi_image, slot_index, attribute, unused_c, bound_animation_flags, flags.
 * @param to_entry Copy bytes into the entry instead of out of it.
 */
static void plain_fields(FieldResourceEntry* entry, unsigned char* bytes, int to_entry)
{
#define FIELD_BYTES(member, at)                                                                                                                                \
    (to_entry ? memcpy(&entry->member, bytes + (at), sizeof(entry->member)) : memcpy(bytes + (at), &entry->member, sizeof(entry->member)))
    FIELD_BYTES(multi_image, 0);
    FIELD_BYTES(slot_index, 1);
    FIELD_BYTES(attribute, 2);
    FIELD_BYTES(unused_c, 4);
    FIELD_BYTES(bound_animation_flags, 6);
    FIELD_BYTES(flags, 8);
#undef FIELD_BYTES
}

void lom_native_field_resource_overlap_pull(void)
{
    for (int index = FIRST_OVERLAPPING_ENTRY; index <= LAST_ENTRY; index++)
    {
        unsigned base = (unsigned)(index - FIRST_OVERLAPPING_ENTRY) * PS1_ENTRY_SIZE + PS1_ENTRY_PLAIN_OFFSET;
        unsigned char bytes[PS1_ENTRY_PLAIN_SIZE];

        for (unsigned i = 0; i < PS1_ENTRY_PLAIN_SIZE; i++)
        {
            bytes[i] = base + i < OVERLAP_END ? *overlap_byte(base + i) : 0;
        }

        plain_fields(&g_field_resource_entries[index], bytes, 1);
    }
}

void lom_native_field_resource_overlap_push(s32 entry_end)
{
    for (int index = FIRST_OVERLAPPING_ENTRY; index <= LAST_ENTRY && index < entry_end; index++)
    {
        unsigned base = (unsigned)(index - FIRST_OVERLAPPING_ENTRY) * PS1_ENTRY_SIZE + PS1_ENTRY_PLAIN_OFFSET;
        unsigned char bytes[PS1_ENTRY_PLAIN_SIZE];

        plain_fields(&g_field_resource_entries[index], bytes, 0);

        for (unsigned i = 0; i < PS1_ENTRY_PLAIN_SIZE && base + i < OVERLAP_END; i++)
        {
            *overlap_byte(base + i) = bytes[i];
        }
    }
}

#endif
