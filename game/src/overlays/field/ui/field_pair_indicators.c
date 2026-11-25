#include "common.h"
#include "../internal/field_actor.h"
#include "../internal/field_calls.h"
#include "../internal/field_contact_geometry.h"
#include "../internal/field_effect_render_state.h"
#include "overlays/field/field_object_state.h"
#include "main/field_runtime.h"
#include "common/gpu_packet.h"
#include <libgpu.h>
#include "../internal/field_modal_runtime.h"
#include "lom_native_gpu_packets.h"

#define FIELD_PAIR_RANGE 32

#define FIELD_PAIR_EXCLUDED_FLAGS 0x23E4

#define FIELD_PAIR_LIST_END 0xFF

#define FIELD_INDICATOR_IDLE -2

#define FIELD_INDICATOR_DONE -1

#define FIELD_INDICATOR_FRAMES 384

#define FIELD_INDICATOR_FADE_START 256

#define FIELD_INDICATOR_OT_INDEX 3

#define FIELD_ELEMENT_AT(base, index) ((void*)((index) * sizeof(*(base)) + (uintptr_t)(base)))

typedef union
{
    u16 word;

    struct
    {
        u8 u;
        u8 v;
    } c;
} FieldIndicatorUv;

typedef struct
{
    GPU_PACKET_TAG(u_long);
    u8 r0;
    u8 g0;
    u8 b0;
    u8 code;
    u32 xy0;
    FieldIndicatorUv uv0;
    u16 clut;
    u32 xy1;
    FieldIndicatorUv uv1;
    u16 tpage;
    u32 xy2;
    FieldIndicatorUv uv2;
    u16 pad1;
    u32 xy3;
    FieldIndicatorUv uv3;
    u16 pad2;
} FieldIndicatorQuad;

static void field_draw_pair_indicator(s32 slot, FieldRenderHalf* render_half);

extern u32 g_field_pair_indicator_corners[];

/**
 * @brief Find the close party pairs of this frame and draw their indicators.
 *
 * With two party members present they form indicator 0; with three, each
 * member pairs with the next one in g_field_party_hud_order (indicators 1-3).
 *
 * @param render_half Render half receiving the indicator primitives.
 */
void field_update_pair_indicators(FieldRenderHalf* render_half)
{
    s32 active[FIELD_PARTY_COUNT];
    s32 active_count;
    s32 index;
    s32 pair;
    s32 distance;

    g_field_pair_indicator_count = 0;
    g_field_pair_indicator_list[0] = FIELD_PAIR_LIST_END;

    if ((g_field_pair_indicators_disabled == 0) && (g_field_scene_mode_bit != 0) && (g_field_active_group != 0))
    {
        for (index = 0, active_count = 0; index < FIELD_PARTY_COUNT; index++)
        {
            if (g_field_actors[index].presence != FIELD_ACTOR_UNUSED)
            {
                active[active_count++] = index;
            }
        }

        if (active_count >= 2)
        {
            if (active_count == 2)
            {
                distance = FIELD_PAIR_RANGE;

                if (!(g_field_object_states[active[0]].flags & FIELD_PAIR_EXCLUDED_FLAGS))
                {
                    distance = active[1];

                    if (g_field_object_states[distance].flags & FIELD_PAIR_EXCLUDED_FLAGS)
                    {
                        distance = FIELD_PAIR_RANGE;
                    }
                    else
                    {
                        distance = field_get_position_distance((VECTOR*)&g_field_actors[active[0]], (VECTOR*)&g_field_actors[distance]);
                    }
                }

                if (distance < FIELD_PAIR_RANGE)
                {
                    if (g_field_pair_indicator_counters[0] == FIELD_INDICATOR_IDLE)
                    {
                        g_field_pair_indicator_counters[0] = 0;
                    }

                    if (g_field_pair_indicator_counters[0] != FIELD_INDICATOR_DONE)
                    {
                        g_field_pair_indicator_list[2] = FIELD_PAIR_LIST_END;
                        g_field_pair_indicator_count = 2;
                        g_field_pair_indicator_list[0] = active[0];
                        g_field_pair_indicator_list[1] = active[1];
                    }
                }
                else
                {
                    g_field_pair_indicator_counters[0] = FIELD_INDICATOR_IDLE;
                }

                field_draw_pair_indicator(0, render_half);
            }
            else
            {
                for (pair = 0; pair < FIELD_PARTY_COUNT; pair++)
                {
                    distance = FIELD_PAIR_RANGE;

                    if (!(g_field_object_states[g_field_party_hud_order[pair]].flags & FIELD_PAIR_EXCLUDED_FLAGS))
                    {
                        distance = g_field_party_hud_order[(pair + 1) % FIELD_PARTY_COUNT];

                        if (g_field_object_states[distance].flags & FIELD_PAIR_EXCLUDED_FLAGS)
                        {
                            distance = FIELD_PAIR_RANGE;
                        }
                        else
                        {
                            distance = field_get_position_distance((VECTOR*)&g_field_actors[g_field_party_hud_order[pair]], (VECTOR*)&g_field_actors[distance]);
                        }
                    }

                    if ((distance < FIELD_PAIR_RANGE) && (g_field_actors[g_field_party_hud_order[pair]].presence != FIELD_ACTOR_UNUSED) &&
                        (g_field_actors[g_field_party_hud_order[(pair + 1) % FIELD_PARTY_COUNT]].presence != FIELD_ACTOR_UNUSED))
                    {
                        if (g_field_pair_indicator_counters[pair] == FIELD_INDICATOR_IDLE)
                        {
                            g_field_pair_indicator_counters[pair] = 0;
                        }

                        if (g_field_pair_indicator_counters[pair] != FIELD_INDICATOR_DONE)
                        {
                            g_field_pair_indicator_list[g_field_pair_indicator_count << 1] = g_field_party_hud_order[pair];
                            g_field_pair_indicator_list[(g_field_pair_indicator_count << 1) + 1] = g_field_party_hud_order[(pair + 1) % FIELD_PARTY_COUNT];
                            g_field_pair_indicator_count++;
                        }
                    }
                    else
                    {
                        g_field_pair_indicator_counters[pair] = FIELD_INDICATOR_IDLE;
                    }

                    field_draw_pair_indicator(pair + 1, render_half);
                }
            }

            g_field_pair_indicator_list[g_field_pair_indicator_count << 1] = FIELD_PAIR_LIST_END;
        }
    }
}

/**
 * @brief Frame counter of pair indicator @p slot.
 * @param counters g_field_pair_indicator_counters.
 * @param slot Indicator 0-3; indicators 0 and 1 share the first counter.
 * @return Address of the indicator's frame counter.
 */
static inline s32* field_get_pair_indicator_counter(s32* counters, s32 slot)
{
    s32* counter;

    counter = counters;

    if (slot != 0)
    {
        counter = FIELD_ELEMENT_AT(counters, slot - 1);
    }

    return counter;
}

/**
 * @brief Advance one pair indicator and draw it as a fading animated quad.
 * @param slot Indicator 0-3; indicators 0 and 1 share the first frame counter.
 * @param render_half Render half whose ordering table and packet cursor receive the quad.
 */
static void field_draw_pair_indicator(s32 slot, FieldRenderHalf* render_half)
{
    FieldIndicatorQuad* quad;
    s32 uv_offset;
    s32 frame;
    s32 brightness;
    u8 swap;
    u8 uv_flags;
    OT_TYPE* ordering_table;
    s32* counters;

    quad = (FieldIndicatorQuad*)render_half->primitive_cursor;
    ordering_table = render_half->ordering_table;
    counters = g_field_pair_indicator_counters;
    frame = *field_get_pair_indicator_counter(counters, slot);

    if (frame != FIELD_INDICATOR_IDLE && frame != FIELD_INDICATOR_DONE)
    {
        if (frame >= FIELD_INDICATOR_FRAMES)
        {
            *field_get_pair_indicator_counter(counters, slot) = FIELD_INDICATOR_DONE;
            return;
        }

        if (g_field_text_session_active == 0)
        {
            *field_get_pair_indicator_counter(counters, slot) = *field_get_pair_indicator_counter(counters, slot) + 1;
        }

        if (frame > FIELD_INDICATOR_FADE_START)
        {
            brightness = 255 - ((frame - FIELD_INDICATOR_FADE_START) * 2);
        }
        else
        {
            brightness = 255;
        }

        if (brightness < 0)
        {
            brightness = 0;
        }

        if (brightness > 255)
        {
            brightness = 256;
        }

        lom_gpu_set_length(quad, 9);
        SET_BGR0(quad, brightness, brightness, brightness);
        lom_gpu_set_code(quad, 0x2E);
        quad->xy0 = g_field_pair_indicator_corners[slot * 4];
        quad->xy1 = g_field_pair_indicator_corners[slot * 4 + 1];
        quad->xy2 = g_field_pair_indicator_corners[slot * 4 + 2];
        quad->xy3 = g_field_pair_indicator_corners[slot * 4 + 3];
        uv_flags = g_field_ribbon_frame_flags[frame % FIELD_RIBBON_FRAME_COUNT];
        uv_offset = (uv_flags & FIELD_RIBBON_UV_VARIANT) * 4;
        quad->uv0.word = g_field_ribbon_uv_corners[uv_offset];
        quad->uv1.word = g_field_ribbon_uv_corners[uv_offset + 1];
        quad->uv2.word = g_field_ribbon_uv_corners[uv_offset + 2];
        quad->uv3.word = g_field_ribbon_uv_corners[uv_offset + 3];

        if (uv_flags & FIELD_RIBBON_FLIP_U)
        {
            swap = quad->uv0.c.u;
            quad->uv0.c.u = quad->uv1.c.u;
            quad->uv1.c.u = swap;
            swap = quad->uv2.c.u;
            quad->uv2.c.u = quad->uv3.c.u;
            quad->uv3.c.u = swap;
        }

        if (uv_flags & FIELD_RIBBON_FLIP_V)
        {
            swap = quad->uv0.c.v;
            quad->uv0.c.v = quad->uv2.c.v;
            quad->uv2.c.v = swap;
            swap = quad->uv1.c.v;
            quad->uv1.c.v = quad->uv3.c.v;
            quad->uv3.c.v = swap;
        }

        quad->tpage = lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_ADD, 448, 0);
        quad->clut = lom_gpu_clut(80, 492);
        lom_gpu_link(&ordering_table[FIELD_INDICATOR_OT_INDEX], quad);
        quad++;
        render_half->primitive_cursor = (u8*)quad;
    }
}
