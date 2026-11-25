#include "overlays/field/internal/field_actor_tables.h"
#include "lom_native_ps1_memory.h"
#include <stdio.h>
#include <string.h>
#include "../../support/lom_check.h"
extern void field_initialize_actor_slots(void), field_clear_actor_slots(void);

extern void field_bind_builtin_animations(void);

extern s32 field_find_free_actor_slot(s32, s32), field_count_free_actor_slots(s32);

extern s32 field_object_has_active_actor_tracks(s32);

extern s32 field_start_builtin_animation(s32, s32, s32);
#define WORD(a) (*(u32*)(uintptr_t)(a))
extern u8 g_field_resource_blob[];
extern void* g_field_builtin_animations;
extern u8* g_field_builtin_track_data;
#define BLOB ((u8*)g_field_resource_blob)
#define ALIAS_COUNT 1
#define BLOB_BASE(alias) ((uintptr_t)BLOB)
#define SET_BUILTIN(base) (g_field_builtin_animations = (void*)(base))

/**
 * @brief Write a 16-bit number.
 * @param p Where.
 * @param n The number.
 */
static void put16(u8* p, u16 n)
{
    memcpy(p, &n, 2);
}

/**
 * @brief Write a 32-bit number.
 * @param p Where.
 * @param n The number.
 */
static void put32(u8* p, u32 n)
{
    memcpy(p, &n, 4);
}

int main(void)
{
    CHECK(lom_native_map_ps1_ram() == 0);
    memset(g_field_actor_slots, 0xA5, 80 * sizeof(FieldActorSlot));
    memset(g_field_object_states, 0xA5, 13 * sizeof(FieldObjectState));
    field_initialize_actor_slots();
    FieldActorSlot expected;
    memset(&expected, 0xA5, sizeof(expected));
    expected.active = expected.part_count = 0;
    expected.duration = 0;
    expected.track_interval = 0;

    for (unsigned i = 0; i < 80; ++i)
    {
        expected.slot_index = i;
        CHECK(!memcmp(&g_field_actor_slots[i], &expected, sizeof(expected)));
    }

    FieldObjectState expected_state;
    memset(&expected_state, 0xA5, sizeof(expected_state));
    expected_state.contact.word = 0xA5A5A5A4u;

    for (unsigned i = 0; i < 13; ++i)
    {
        CHECK(!memcmp(&g_field_object_states[i], &expected_state, sizeof(expected_state)));
    }

    field_clear_actor_slots();
    expected.sound_flags[0] = expected.sound_flags[1] = 0;
    expected.target_count = expected.track_mask = expected.pending_track_mask = 0;

    for (unsigned i = 0; i < 80; ++i)
    {
        expected.slot_index = i;
        CHECK(!memcmp(&g_field_actor_slots[i], &expected, sizeof(expected)));
    }

    memset(g_field_actor_bindings, 0, 3 * sizeof(FieldActorBinding));
    CHECK(field_count_free_actor_slots(0) == 48 && field_find_free_actor_slot(0, 1) == 0);

    for (unsigned i = 0; i < 48; ++i)
    {
        g_field_actor_slots[i].active = 1;
    }

    CHECK(field_count_free_actor_slots(0) == 0 && field_find_free_actor_slot(0, 0) == -1);
    g_field_actor_slots[47].active = 0;
    CHECK(field_count_free_actor_slots(0) == 1 && field_find_free_actor_slot(0, 1) == 47);
    g_field_actor_bindings[0].state = FIELD_BINDING_LOADING;
    g_field_actor_bindings[0].slot = 47;
    CHECK(field_find_free_actor_slot(0, 1) == -1 && field_find_free_actor_slot(1, 1) == -1);
    CHECK(field_count_free_actor_slots(0) == 1);
    g_field_actor_slots[7].active = 0;
    CHECK(field_find_free_actor_slot(0, 0) == 7 && field_find_free_actor_slot(1, 1) == 7);
    g_field_actor_bindings[1].state = FIELD_BINDING_READY;
    g_field_actor_bindings[1].slot = 7;
    CHECK(field_find_free_actor_slot(2, 1) == -1);
    g_field_actor_slots[0].active = 0;
    CHECK(field_find_free_actor_slot(2, 1) == 0);
    g_field_actor_bindings[2].state = FIELD_BINDING_LOADING;
    g_field_actor_bindings[2].slot = 0;
    CHECK(field_find_free_actor_slot(12, 1) == -1 && field_find_free_actor_slot(12, 0) == -1);
    g_field_actor_slots[1].active = 0;
    CHECK(field_find_free_actor_slot(12, 0) == 1 && field_find_free_actor_slot(12, 1) == -1);
    field_clear_actor_slots();

    for (unsigned i = 0; i < 80; ++i)
    {
        g_field_actor_slots[i].owner_object_index = 0xFF;
    }

    g_field_actor_slots[48].owner_object_index = 4;
    g_field_actor_slots[48].track_mask = 1;
    CHECK(field_object_has_active_actor_tracks(4) == 0);
    g_field_actor_slots[6].owner_object_index = 4;
    g_field_actor_slots[6].pending_track_mask = 1;
    CHECK(field_object_has_active_actor_tracks(4) == 0);
    g_field_actor_slots[6].track_mask = 1;
    CHECK(field_object_has_active_actor_tracks(4) == 1 && field_object_has_active_actor_tracks(5) == 0);
    CHECK(g_field_actor_slots[6].active == 0);

    u8* blob = BLOB;
    memset(blob, 0, 0x400);
    put32(blob + 4, 0x180);
    put32(blob + 8, 0x200);
    put16(blob + 0x20, 0x50);
    put16(blob + 0x22, 0x100);
    put16(blob + 0x24, 0x160);
    put16(blob + 0x26, 23);
    blob[0x28] = 2;
    blob[0x200] = 2;
    put16(blob + 0x336 + 18, 91);
    field_bind_builtin_animations();
    CHECK((u8*)g_field_builtin_animations == blob && g_field_builtin_track_data == blob + 0x180);
    g_field_object_states[5].action = 19;
    FieldActorSlot* slot = &g_field_actor_slots[27];

    for (unsigned alias = 0; alias < ALIAS_COUNT; ++alias)
    {
        for (unsigned own_duration = 0; own_duration < 2; ++own_duration)
        {
            uintptr_t base = BLOB_BASE(alias);
            SET_BUILTIN(base);
            put16(blob + 0x336 + 12, own_duration ? 0x8900 : 0x0900);
            memset(slot, 0xA5, sizeof(*slot));
            slot->slot_index = 27;
            static u8 pool_before[80 * sizeof(FieldActorSlot)];
            memcpy(pool_before, g_field_actor_slots, sizeof(pool_before));
            memcpy(&expected, slot, sizeof(expected));
            expected.parts = (FieldObjectPart*)(base + 0x50);
            expected.curves = (FieldParameterCurve*)(base + 0x100);
            expected.curve_segments = (u16*)(base + 0x160);
            expected.animation = (FieldAnimationDef*)(base + 0x336);
            expected.active = 1;
            expected.part_count = 2;
            expected.hit_reaction = 19;
            expected.animation_index = expected.sequence_active = 0;
            expected.duration = own_duration ? 91 : 23;
            put32((u8*)&expected.status, 0x000200BE);
            expected.owner_object_index = 5;
            expected.track_interval = 0;
            u8 blob_before[0x400];
            memcpy(blob_before, blob, sizeof(blob_before));
            put16(blob_before + 0x336 + 12, own_duration ? 0x8100 : 0x0100);
            CHECK(field_start_builtin_animation(5, 27, 2) == 1);
            CHECK(!memcmp(slot, &expected, sizeof(expected)) && !memcmp(blob, blob_before, sizeof(blob_before)));
            memcpy(pool_before + 27 * sizeof(FieldActorSlot), &expected, sizeof(expected));
            CHECK(!memcmp(g_field_actor_slots, pool_before, sizeof(pool_before)));
            CHECK(slot->parts == (FieldObjectPart*)(uintptr_t)(base + 0x50));
            CHECK(slot->animation->duration == 91);
            CHECK(field_start_builtin_animation(5, 27, 0) == 0);
            expected.part_count = 0;
            CHECK(!memcmp(slot, &expected, sizeof(expected)));
            slot->part_count = 3;
            CHECK(field_start_builtin_animation(5, 27, 3) == 0);
            CHECK(!memcmp(slot, &expected, sizeof(expected)));
        }
    }

    puts("LoM FIELD slots smoke passed: exact 80-slot reset, neighbor guards, allocation/binding rules, track query, builtin pointers/both aliases, duration "
         "and empty bindings.");
    return 0;
}
