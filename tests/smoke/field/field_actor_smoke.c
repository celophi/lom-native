#include "overlays/field/internal/field_actor_tables.h"
#include "overlays/field/internal/field_path_interpolation.h"
#include "overlays/field/internal/field_effect_dispatch.h"
#define rename lom_ps1_rename
#include "main/main.h"
#undef rename
#include "lom_native_ps1_memory.h"
#include "lom_native_random.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
extern void field_initialize_actor_record(s32, s32);

extern void field_initialize_actor_part(s32, s32);

extern void field_restart_actor_animation(FieldActor*);

extern void field_restart_actor_animation_reverse(FieldActor*);

extern u8* field_advance_actor_animation_frame(FieldActor*);

extern u8* field_advance_actor_part_animation_frame(FieldActor*, u8*);

extern s32* field_render_effect_frame16(FieldActor*, s32*, s32*, u8*, s32, FieldObjectPart*);

extern s32 field_evaluate_parameter_track(FieldActorSlot*, s32);

extern int lom_test_field_idle_oracle(FieldActor*, u8*);

extern s32 g_field_track_index, g_field_text_session_active;
#define WORD(a) (*(u32*)(uintptr_t)(a))
#include "overlays/field/internal/field_actor_runtime.h"
#include "../../support/lom_check.h"
extern s32 g_field_path_segment_steps, g_field_path_order, g_field_path_length, g_field_path_point_count, g_field_path_half_order, g_field_path_basis_count,
    g_field_path_knot_count, g_field_path_degree, g_field_path_generated_count;
extern u8 g_field_path_groups[];
#define RAM(address) ((u8*)MEMORY_MAP_RAM(address))
#define LOW_RAM(address) RAM(address)
#define ALIAS_COUNT 1
#define RESOURCE_AT(alias) (data)
#define FRAME(value) ((uintptr_t)(value))

/**
 * @brief A pointer filled with 0xA5, to show a pointer that's never been set.
 * @return The pointer.
 */
static uintptr_t filled_pointer(void)
{
    uintptr_t value;
    memset(&value, 0xA5, sizeof(value));
    return value;
}

#define FILLED_POINTER filled_pointer()

int main(void)
{
    CHECK(lom_native_map_ps1_ram() == 0);
    FieldActorSlot* slot = (void*)LOW_RAM(0x801E9000u);
    FieldParameterCurve* curve = (void*)LOW_RAM(0x801E9300u);
    u16* segment = (void*)LOW_RAM(0x801E9400u);
    memset(slot, 0, sizeof(*slot));
    *curve = (FieldParameterCurve){.head.word = 0x81, .start_value = 100, .end_value = 356};
    *segment = (32 << 10) | 8;
    slot->curves = curve;
    slot->curve_segments = segment;
    slot->track_frames[0] = 4;
    g_field_track_index = g_field_text_session_active = 0;
    const s32 curve_goldens[] = {165, 122, 139, 168, 221, 121, 189, 128};
    lom_native_srand(1);
    srand(923);

    for (unsigned i = 0; i < sizeof(curve_goldens) / sizeof(curve_goldens[0]); ++i)
    {
        (void)rand();
        CHECK(field_evaluate_parameter_track(slot, 0) == curve_goldens[i]);
    }

    lom_native_srand(1);
    g_field_text_session_active = 1;
    CHECK(field_evaluate_parameter_track(slot, 0) == 228);
    g_field_text_session_active = 0;
    CHECK(field_evaluate_parameter_track(slot, 0) == curve_goldens[0]);
    CHECK(lom_native_rand() == 5758);
    g_field_path_segment_steps = 20;
    g_field_path_order = 4;
    g_field_path_length = 201;
    g_field_path_point_count = 10;
    g_field_path_half_order = 2;
    g_field_path_basis_count = 13;
    g_field_path_knot_count = 16;
    g_field_path_degree = 3;
    g_field_path_generated_count = 10;
    const s16 path_x[11] = {0, 60, 120, 180, 240, 300, 360, 420, 480, 540, 0};
    const s16 path_z[11] = {60, 120, 180, 240, 300, 360, 420, 480, 540, 0, 0};
    const s16 path_goldens[][2] = {{98, 58},   {58, 117},  {117, 177}, {177, 237}, {237, 297}, {297, 357},
                                   {357, 417}, {417, 477}, {477, 438}, {438, 98},  {98, 58}};
    FieldMotionRecord effect = {0};

    for (unsigned group = 0; group < 32; ++group)
    {
        u8* points = g_field_path_groups + group * 44;
        memcpy(points, path_x, sizeof(path_x));
        memcpy(points + 22, path_z, sizeof(path_z));

        for (unsigned knot = 0; knot < 11; ++knot)
        {
            u8 time = knot * 20;
            field_update_path_position(&time, &effect, group);
            CHECK(effect.x == path_goldens[knot][0] * 256 && effect.z == path_goldens[knot][1] * 256);
        }

        for (unsigned frame = 0; frame < 201; ++frame)
        {
            u8 time = frame;
            field_update_path_position(&time, &effect, group);
            CHECK(time == frame && effect.x >= 0 && effect.x <= 540 * 256 && effect.z >= 0 && effect.z <= 540 * 256);
        }

        u8 time = 201;
        field_update_path_position(&time, &effect, group);
        CHECK(time == 0 && effect.x == 98 * 256 && effect.z == 58 * 256);
    }

    memset(g_field_actors, 0xA5, 13 * sizeof(FieldActor));
    memset(g_field_object_states, 0xA5, 13 * sizeof(FieldObjectState));
    memset(g_field_object_parts, 0xA5, 13 * sizeof(FieldObjectPart));
    g_field_resource_entries[3].slot_index = 7;
    field_initialize_actor_record(3, 3);
    FieldActor* actor = &g_field_actors[3];
    CHECK(actor->x == (s32)0xFFFB0000 && actor->y == 0 && actor->z == 0);
    CHECK(actor->object_index == 3 && actor->resource_index == 3 && actor->texture_slot == 7);
    CHECK(actor->owner_slot == 51 && actor->script_index == 0xFF && actor->presence == 0);
    CHECK(actor->control.word == 2 && actor->frame_timer == 1 && actor->animation_active == 0);
    CHECK(actor->tint_red == 128 && actor->tint_green == 128 && actor->tint_blue == 128);
    CHECK((uintptr_t)g_field_object_states[3].collision_node == UINT32_MAX && g_field_object_states[3].collision_flags == 0);
    CHECK(g_field_object_states[3].contact.word == (0xA5A5A5A5u & ~0xC0u));
    CHECK(g_field_actors[2].x == (s32)0xA5A5A5A5u && g_field_actors[4].x == (s32)0xA5A5A5A5u);
    CHECK((uintptr_t)g_field_object_states[2].collision_node == FILLED_POINTER && (uintptr_t)g_field_object_states[4].collision_node == FILLED_POINTER);

    for (unsigned small = 0; small < 2; ++small)
    {
        field_initialize_actor_part(3, small);
        CHECK(g_field_object_parts[3].appearance.fields.scale_xz == (small ? 0x30 : 0x40));
        CHECK(g_field_object_parts[3].scale_y == (small ? 0x30 : 0x40));
        CHECK(g_field_object_parts[3].tint_red == 128 && g_field_object_parts[3].track_flags.bits.rotation_axis == 2);
        CHECK(g_field_object_parts[2].track_flags.word == 0xA5A5A5A5u && g_field_object_parts[4].track_flags.word == 0xA5A5A5A5u);
    }

    g_field_player_records[1].character_kind = FIELD_PLAYER_KIND_HERO;
    field_initialize_actor_record(1, 3);
    CHECK((g_field_actors[1].control.word & 0x780000) == 0x500000);
    g_saved_game_ctx = (SavedGameLayout*)RAM(0x80160000u);
    ((SavedGameLayout*)RAM(0x80160000u))->joined_golem = 1;
    g_field_player_records[2].character_id = 0x41;
    field_initialize_actor_record(2, 3);
    CHECK((g_field_actors[2].control.word & 0x30000) == 0x20000);

    u8* data = RAM(0x80170000u);
    memset(data, 0, 128);
    data[2] = 0x40;
    data[4] = 2;
    data[6] = 0x10;
    data[8] = 0x20;
    data[9] = 0x80;
    const u8 simple[] = {2, 0, 2, 1, 3};
    const u8 motion[] = {2, 0, 2, 6, 4, 1, 3, 9, 5};
    memcpy(data + 0x10, simple, sizeof(simple));
    memcpy(data + 0x20, motion, sizeof(motion));
    data[0x42] = 0x60;
    data[0x44] = 0x70;

    for (unsigned alias = 0; alias < ALIAS_COUNT; ++alias)
    {
        for (unsigned wrap = 0; wrap < 2; ++wrap)
        {
            for (unsigned extended = 0; extended < 2; ++extended)
            {
                g_field_resource_entries[3].start = RESOURCE_AT(alias);
                g_field_resource_entries[3].end = (u8*)(uintptr_t)0xDEADBEEFu;
                data[5] = wrap;
                uintptr_t base = wrap ? (uintptr_t)FIELD_FRAME_DATA_MARK_16BIT(data) : (uintptr_t)data;
                actor->animation = extended;
                actor->animation_active = 1;
                actor->animation_state = 0;
                field_restart_actor_animation(actor);
                CHECK(FRAME(actor->frame_data) == base + 0x60 && actor->frame_timer == 2);
                CHECK(actor->height == (extended ? 6 : 0) && actor->next_height == (extended ? 9 : 0));
                CHECK((uintptr_t)field_advance_actor_animation_frame(actor) == base + 0x60);
                CHECK(actor->animation_frame == 0 && actor->frame_timer == 1);
                actor->frame_data = FRAME((uintptr_t)field_advance_actor_animation_frame(actor));
                CHECK(FRAME(actor->frame_data) == base + 0x70 && actor->animation_frame == 1 && actor->frame_timer == 3);
                CHECK(actor->height == (extended ? 9 : 0) && actor->speed_accumulator == (extended ? 5 : 0));

                for (unsigned i = 0; i < 3; ++i)
                {
                    actor->frame_data = FRAME((uintptr_t)field_advance_actor_animation_frame(actor));
                }

                CHECK(FRAME(actor->frame_data) == base + 0x60 && actor->animation_frame == 0);
                field_restart_actor_animation_reverse(actor);
                CHECK(FRAME(actor->frame_data) == base + 0x70 && actor->animation_frame == 1);
                CHECK(actor->control.word & FIELD_CONTROL_PLAY_ONCE);

                for (unsigned i = 0; i < 3; ++i)
                {
                    CHECK((uintptr_t)field_advance_actor_animation_frame(actor) == base + 0x70);
                }

                CHECK(actor->animation_frame == 1 && actor->frame_timer == 1);
                actor->animation = 0xFF;
                actor->animation_active = 0;
                field_restart_actor_animation(actor);
                CHECK(actor->animation == 0x80 && FRAME(actor->frame_data) == base + 0x60 && actor->frame_timer == 1);
                CHECK((uintptr_t)field_advance_actor_animation_frame(actor) == base + 0x60 && actor->frame_timer == 1);
            }
        }
    }

    CHECK(!lom_test_field_idle_oracle(actor, data));
    const u8 quad[] = {1, 0xF6, 0xFF, 0xEC, 0xFF, 30, 0, 0xEC, 0x22, 0xFF, 0xF6, 0xFF, 40, 0, 30, 0, 40, 0};
    memcpy(data + 0x60, quad, sizeof(quad));
    s32* packets = (void*)LOW_RAM(0x80190000u);
    s32* ordering_table = (void*)LOW_RAM(0x80194000u);
    const uintptr_t frames[] = {(uintptr_t)FIELD_FRAME_DATA_MARK_16BIT(data + 0x60), (uintptr_t)(data + 0x60)};

    for (unsigned facing = 0; facing < 2; ++facing)
    {
        for (unsigned view = 0; view < sizeof(frames) / sizeof(frames[0]); ++view)
        {
            actor->animation = facing ? FIELD_ANIMATION_FACING : 0;
            actor->height = actor->next_height = 0;
            g_field_object_parts[3].effect_flags.word = 0;
            g_field_object_parts[3].spawn_flags.word = 0;
            CHECK(field_render_effect_frame16(actor, packets, ordering_table, (u8*)frames[view], 0, &g_field_object_parts[3]) == packets);
            CHECK(g_field_object_states[3].bounds.half.left == (facing ? -30 : -10));
            CHECK(g_field_object_states[3].bounds.half.top == -20);
            CHECK(g_field_object_states[3].bounds.half.right == (facing ? -30 : -10));
            CHECK(g_field_object_states[3].bounds.half.bottom == 40);
        }
    }

    const uintptr_t resources[] = {(uintptr_t)data};
#define PART_FRAME(wrap) ((wrap) ? (uintptr_t)FIELD_FRAME_DATA_MARK_16BIT(data + 0x60) : (uintptr_t)(data + 0x60))

    for (unsigned wrap = 0; wrap < 2; ++wrap)
    {
        for (unsigned view = 0; view < sizeof(resources) / sizeof(resources[0]); ++view)
        {
            data[5] = wrap;
            *actor = (FieldActor){.frame_timer = 1, .frame_length = 1};
            CHECK((uintptr_t)field_advance_actor_part_animation_frame(actor, (u8*)resources[view]) == PART_FRAME(wrap));
        }
    }

    for (unsigned i = 0; i < 256; ++i)
    {
        FIELD_EFFECT_ACTOR_RECORDS[i].presence = FIELD_ACTOR_UNUSED;
    }

    FieldActorSlot* owner = &g_field_actor_slots[3];
    memset(owner, 0, sizeof(*owner));
    owner->owner_object_index = 3;
    owner->parts = (void*)RAM(0x801B0000u);

    for (unsigned i = 0; i < 2; ++i)
    {
        owner->parts[i] = g_field_object_parts[3];
    }

    g_field_resource_entries[3].start = (void*)LOW_RAM(0x80170000u);
    actor->resource_index = 3;
    FieldRenderContext* render = (void*)LOW_RAM(0x801C0000u);
    memset(render, 0, sizeof(*render));
    const u8 quad8[] = {1, 0xF6, 0xEC, 30, 0xEC, 0xF6, 40, 30, 0x22, 40};

    for (unsigned wrap = 0; wrap < 2; ++wrap)
    {
        for (unsigned part = 0; part < 2; ++part)
        {
            data[5] = wrap;
            memcpy(data + 0x60, wrap ? quad : quad8, wrap ? sizeof(quad) : sizeof(quad8));
            FIELD_EFFECT_ACTOR_RECORDS[0] =
                (FieldActor){.presence = 2, .owner_slot = 3, .owner_part = part, .object_index = 3, .resource_index = 3, .frame_timer = 1, .frame_length = 1};
            render->packet_cursor = packets;
            field_render_effects(render);
            CHECK(render->packet_cursor == packets);
            CHECK(g_field_object_states[3].linked_effect_index == 0);
            CHECK(g_field_object_states[3].bounds.half.left == -10);
            CHECK(g_field_object_states[3].bounds.half.top == -20);
            CHECK(g_field_object_states[3].bounds.half.bottom == 40);
            CHECK((uintptr_t)owner->parts == (uintptr_t)RAM(0x801B0000u));
        }
    }

    puts("LoM FIELD actor smoke passed: retail random curves/host isolation, closed cubic paths/all groups, fixed strides, actor/part init, partner/companion "
         "flags, both RAM aliases and frame formats, forward/reverse/held animations, physical-address 16-bit frames, part-animation pointer/format decoding "
         "and high-address owner-sprite parts.");
    return 0;
}
