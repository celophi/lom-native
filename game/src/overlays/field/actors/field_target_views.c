#if defined(LOM_NATIVE_POINTERS)

#include "overlays/field/internal/field_actor.h"
#include "overlays/field/internal/field_effect_types.h"
#include "overlays/field/field_actor_records.h"
#include "lom_native_unit_decode.h"

void lom_unit_decode_element_g_field_actors(const unsigned char* source, void* destination);
void lom_unit_decode_element_g_field_effect_records(const unsigned char* source, void* destination);
void lom_unit_decode_element_g_field_object_states(const unsigned char* source, void* destination);
void lom_unit_decode_element_g_field_object_parts(const unsigned char* source, void* destination);
void lom_unit_encode_element_g_field_actors(const void* source, unsigned char* destination);
void lom_unit_encode_element_g_field_object_states(const void* source, unsigned char* destination);

#define PS1_ACTOR_SIZE 84
#define PS1_OBJECT_STATE_SIZE 572
#define PS1_OBJECT_PART_SIZE 72

#define VIEW_COUNT 4

/**
 * @brief Decode the record the PS1 reads at an index past an array's end.
 * @param array_start The array's first element (natively).
 * @param index Index the game used.
 * @param ps1_size The record's size on the PS1.
 * @param decode One-element decoder.
 * @param view Where to decode it.
 */
static void* ps1_view(const void* array_start, s32 index, unsigned ps1_size, void (*decode)(const unsigned char*, void*), void* view)
{
    unsigned char bytes[PS1_OBJECT_STATE_SIZE];
    unsigned address = lom_unit_ps1_address(array_start) + (unsigned)index * ps1_size;

    lom_unit_read_ps1(address, bytes, ps1_size);
    lom_unit_decode_lenient(decode, bytes, view);
    return view;
}

struct FieldActor* lom_native_field_actor_view(s32 index)
{
    static FieldActor views[VIEW_COUNT];
    static unsigned next;

    if ((u32)index < FIELD_ACTOR_COUNT)
    {
        return &g_field_actors[index];
    }

    return ps1_view(g_field_actors, index, PS1_ACTOR_SIZE, lom_unit_decode_element_g_field_actors, &views[next++ % VIEW_COUNT]);
}

FieldMotionRecord* lom_native_field_motion_view(s32 index)
{
    static FieldMotionRecord views[VIEW_COUNT];
    static unsigned next;

    if ((u32)index < FIELD_ACTOR_COUNT)
    {
        return &FIELD_ACTOR_MOTION_RECORDS[index];
    }

    return ps1_view(g_field_actors, index, PS1_ACTOR_SIZE, lom_unit_decode_element_g_field_effect_records, &views[next++ % VIEW_COUNT]);
}

FieldObjectState* lom_native_field_object_state_view(s32 index)
{
    static FieldObjectState views[VIEW_COUNT];
    static unsigned next;

    if ((u32)index < FIELD_OBJECT_COUNT)
    {
        return &g_field_object_states[index];
    }

    return ps1_view(g_field_object_states, index, PS1_OBJECT_STATE_SIZE, lom_unit_decode_element_g_field_object_states, &views[next++ % VIEW_COUNT]);
}

FieldObjectPart* lom_native_field_object_part_view(s32 index)
{
    static FieldObjectPart views[VIEW_COUNT];
    static unsigned next;

    if ((u32)index < FIELD_ACTOR_COUNT)
    {
        return &g_field_object_parts[index];
    }

    return ps1_view(g_field_object_parts, index, PS1_OBJECT_PART_SIZE, lom_unit_decode_element_g_field_object_parts, &views[next++ % VIEW_COUNT]);
}

static FieldActor s_actor_write_view;
static FieldObjectState s_object_state_write_view;

struct FieldActor* lom_native_field_actor_for_write(s32 index)
{
    if ((u32)index < FIELD_ACTOR_COUNT)
    {
        return &g_field_actors[index];
    }

    return ps1_view(g_field_actors, index, PS1_ACTOR_SIZE, lom_unit_decode_element_g_field_actors, &s_actor_write_view);
}

void lom_native_field_actor_commit(s32 index, struct FieldActor* view)
{
    unsigned char bytes[PS1_ACTOR_SIZE];

    if ((u32)index < FIELD_ACTOR_COUNT)
    {
        return;
    }

    lom_unit_encode_lenient(lom_unit_encode_element_g_field_actors, view, bytes);
    lom_unit_write_ps1(lom_unit_ps1_address(g_field_actors) + (unsigned)index * PS1_ACTOR_SIZE, bytes, PS1_ACTOR_SIZE);
}

FieldObjectState* lom_native_field_object_state_for_write(s32 index)
{
    if ((u32)index < FIELD_OBJECT_COUNT)
    {
        return &g_field_object_states[index];
    }

    return ps1_view(g_field_object_states, index, PS1_OBJECT_STATE_SIZE, lom_unit_decode_element_g_field_object_states, &s_object_state_write_view);
}

void lom_native_field_object_state_commit(s32 index, FieldObjectState* view)
{
    unsigned char bytes[PS1_OBJECT_STATE_SIZE];

    if ((u32)index < FIELD_OBJECT_COUNT)
    {
        return;
    }

    lom_unit_encode_lenient(lom_unit_encode_element_g_field_object_states, view, bytes);
    lom_unit_write_ps1(lom_unit_ps1_address(g_field_object_states) + (unsigned)index * PS1_OBJECT_STATE_SIZE, bytes, PS1_OBJECT_STATE_SIZE);
}

#endif
