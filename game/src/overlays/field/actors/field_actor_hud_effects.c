#include "common.h"
#include <abs.h>
#include <libgpu.h>
#include <libgte.h>
#include <memory.h>
#include <rand.h>
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"

#include "../internal/field_actor_palette.h"
#include "../internal/field_actor_runtime.h"
#include "../internal/field_actor_tables.h"
#include "../internal/field_calls.h"
#include "../internal/field_effect_render_state.h"
#include "../internal/field_effect_types.h"
#include "common/gpu_packet.h"
#include "common/saved_game.h"
#include "common/tim.h"
#include "common/vector.h"
#include "main/cdrom.h"
#include "main/display.h"
#include "main/field_runtime.h"
#include "main/main.h"
#include "overlays/field/field_actor_records.h"
#include "overlays/field/field_vram.h"

#define FIELD_HUD_PANEL_FRAMES 20

#define FIELD_HUD_HP_PER_BAR 100
#define FIELD_HUD_BOSS_HP_PER_BAR 400

#define FIELD_HUD_SINGLE_X 112
#define FIELD_HUD_PAIR_X 56
#define FIELD_HUD_PAIR_STEP 108
#define FIELD_HUD_TRIO_X 8
#define FIELD_HUD_TRIO_STEP 104
#define FIELD_HUD_TRIO_LOW_Y 28
#define FIELD_HUD_TRIO_HIGH_Y 4
#define FIELD_HUD_PARTY_Y 16
#define FIELD_HUD_BOSS_X 32
#define FIELD_HUD_BOSS_Y 192
#define FIELD_HUD_ENEMY_OFFSET_X 28

#define FIELD_HUD_MARGIN_X 32
#define FIELD_HUD_MIN_Y 16
#define FIELD_HUD_MAX_Y 208

#define FIELD_HUD_ANCHOR_IGNORED_ANIMATION 0x2F

#define FIELD_HUD_SHAKE_FRAMES 6

#define FIELD_HUD_BLINK_FRAMES 6
#define FIELD_HUD_BLINK_CHANCE_MASK 0x3F
#define FIELD_HUD_BLINK_LIST_COUNT 8
#define FIELD_HUD_BLINK_LIST_END 0xFF

#define FIELD_HUD_GLYPH_PERCENT 10
#define FIELD_HUD_GLYPH_ADVANCE 7

#define FIELD_HUD_OT_INDEX 3
#define FIELD_HUD_DELTA_OT_INDEX 2

#define FIELD_HUD_SCRATCH_POSITION ((Vec2s*)SCRATCHPAD_ADDRESS)

#define FIELD_HUD_UV(u, v) (((v) << 8) | (u))

#define FIELD_HUD_CLUT lom_gpu_clut(FIELD_UI_CLUT_X, FIELD_UI_CLUT_Y)
#define FIELD_HUD_BLINK_CLUT lom_gpu_clut(FIELD_UI_CLUT_X, FIELD_UI_CLUT_Y + 1)

#define FIELD_HUD_PARTY_BAR_X 29
#define FIELD_HUD_PARTY_BAR_Y 9
#define FIELD_HUD_BOSS_BAR_X 15
#define FIELD_HUD_BOSS_BAR_Y 6
#define FIELD_HUD_ENEMY_BAR_X 4
#define FIELD_HUD_ENEMY_BAR_Y 6

#define FIELD_HUD_PANEL_WIDTH 88
#define FIELD_HUD_PANEL_HEIGHT 24
#define FIELD_HUD_PANEL_UV FIELD_HUD_UV(0, 0x10)
#define FIELD_HUD_COMPANION_PANEL_UV FIELD_HUD_UV(0, 0x28)
#define FIELD_HUD_BOSS_PANEL_WIDTH 256
#define FIELD_HUD_BOSS_PANEL_HEIGHT 16
#define FIELD_HUD_BOSS_PANEL_UV FIELD_HUD_UV(0, 0)
#define FIELD_HUD_ENEMY_PANEL_WIDTH 64
#define FIELD_HUD_ENEMY_PANEL_HEIGHT 16
#define FIELD_HUD_ENEMY_PANEL_UV FIELD_HUD_UV(0, 0x40)

#define FIELD_HUD_PORTRAIT_SIZE 24
#define FIELD_HUD_PORTRAIT_UV FIELD_HUD_UV(0, 0xE8)
#define FIELD_HUD_COMPANION_CLUT_Y 0x1EF

#define FIELD_HUD_BLINK_X 34
#define FIELD_HUD_BLINK_Y (-6)
#define FIELD_HUD_BLINK_SIZE 16
#define FIELD_HUD_BLINK_UV FIELD_HUD_UV(0x58, 0x20)

#define FIELD_HUD_PERCENT_X 48
#define FIELD_HUD_LINE_X 24
#define FIELD_HUD_LINE_Y 16

#define FIELD_HUD_GLYPH_WIDTH 8
#define FIELD_HUD_GLYPH_HEIGHT 11
#define FIELD_HUD_GLYPH_UV FIELD_HUD_UV(0x58, 0x15)

#define FIELD_HUD_BAR_WIDTH 54
#define FIELD_HUD_BOSS_BAR_WIDTH 227
#define FIELD_HUD_BAR_HEIGHT 3

#define FIELD_HUD_BAR_SLANT 3

#define FIELD_HUD_COLOR_DIM 0x3F3F3F
#define FIELD_HUD_COLOR_MID 0x7F7F7F

#define FIELD_HUD_RISE_MIN_STEP 3
#define FIELD_HUD_FALL_MIN_STEP 4

#define FIELD_HUD_LINE_LENGTH 35

#define FIELD_HUD_RECOVERY_DARK GPU_COLOR_WORD(0x20, 0x20, 0x20)
#define FIELD_HUD_RED GPU_COLOR_WORD(0xFF, 0, 0)

extern u32 g_field_hud_hp_colors[];
extern CVECTOR g_field_hud_status_colors[2];
extern CVECTOR g_field_hud_effect_colors[2];
extern CVECTOR g_field_hud_full_status_colors[2];
extern CVECTOR g_field_hud_companion_status_colors[2];
extern s16 g_field_hud_shake_offsets[];
extern s32 g_field_hud_blink_frames[FIELD_PARTY_COUNT];
extern s32 g_field_hud_bar_height;
extern s32 g_field_hud_bar_width;
extern s32 g_field_hud_bar_offset_x;
extern s32 g_field_hud_bar_offset_y;
#define FIELD_OBJECT_HANDLER_COUNT 16

#define FIELD_OBJECT_HANDLER_NONE 0xFF

#define FIELD_OBJECT_HANDLER_FUNCTION_MIN 0x100

typedef void (*FieldObjectFlagFunction)(FieldActor* actor, s32 flag);

typedef union
{
    u32 animation;
    FieldObjectFlagFunction function;
} FieldObjectFlagHandler;

extern FieldObjectFlagHandler g_field_object_flag_handlers[FIELD_OBJECT_HANDLER_COUNT];

#define FIELD_ANIMATION_PETRIFIED_POSE 0x14

#define FIELD_ANIMATION_SLEEP_POSE 0x1B

#define FIELD_HIT_FLASH_TINT 0x20

#define FIELD_PART_SCALE_HALF 0x20

#define FIELD_FADE_PRIM_COUNT 256

#define FIELD_FADE_PRIM_STEP 0x10

#define FIELD_CONTACT_LINKED_SHIFT 1

#define FIELD_STATUS_ANIMATION_POISON 5
#define FIELD_STATUS_ANIMATION_DARKNESS 6
#define FIELD_STATUS_ANIMATION_PARALYSIS 7
#define FIELD_STATUS_ANIMATION_CONFUSION 8
#define FIELD_STATUS_ANIMATION_FLAME_BURST 11
#define FIELD_STATUS_ANIMATION_HYPNOSIS 10
#define FIELD_STATUS_ANIMATION_SLEEP 9
#define FIELD_STATUS_ANIMATION_PETRIFIED 12

#define FIELD_STATUS_BIT_NONE 0x20

#define START_OBJECT_EFFECT(actor, resource)                                                                                                                   \
    (field_start_builtin_animation((actor)->object_index, (actor)->object_index + FIELD_OBJECT_EFFECT_SLOT_BASE, (resource)),                                  \
     field_start_actor_animation((actor)->object_index + FIELD_OBJECT_EFFECT_SLOT_BASE, 0, 0))

typedef struct
{
    u16 x;
    u16 y;
} ShadowScreenPosition;

typedef struct
{
    s32 x;
    s32 y;
    s32 z;
} ShadowWorldPosition;

#define FIELD_SHADOW_U0 0
#define FIELD_SHADOW_U1 0x40
#define FIELD_SHADOW_V0 0x50
#define FIELD_SHADOW_V1 0x70

#define SHADOW_SCREEN_POSITION ((ShadowScreenPosition*)SCRATCHPAD_AT(0xC0))
#define SHADOW_WORLD_POSITION ((ShadowWorldPosition*)SCRATCHPAD_AT(0xC4))

#define SHADOW_ELEVATION(actor, height) (((actor)->y - ((height) << 8)) >> 11)

extern POLY_FT4 g_field_fade_prims[FIELD_FADE_PRIM_COUNT];
extern u16 g_field_fade_prim_depths[FIELD_FADE_PRIM_COUNT];
extern FieldActor* g_field_updating_actor;
static void field_draw_actor_hud_panel(s32 x, s32 y, s32 slot, FieldRenderHalf* render_half, s32 hp_per_bar);
static u8* field_emit_hud_status_line(LINE_G2* line, FieldRenderHalf* render_half, CVECTOR* colors, s32 intensity, s32 x, s32 y);
static void* field_emit_hud_percentage(void* packet, FieldRenderHalf* render_half, s32 value, Vec2s* position);
static void* field_emit_hud_glyph(SPRT* sprite, FieldRenderHalf* render_half, s32 value, Vec2s* position);
static POLY_F4* field_emit_hud_damage_quad(POLY_F4* prim, s32 y, OT_TYPE* ot);
static POLY_F4* field_emit_hud_healing_quad(POLY_F4* prim, s32 y, OT_TYPE* ot);
static inline void field_clear_link_target_flag(s32 object_index);
static inline void field_dispatch_object_flag(s32 index, FieldObjectState* state, s32 pending_flags);

/**
 * @brief Set the corners of a slanted gauge bar.
 * @param bar Gauge quad.
 * @param x Panel screen x.
 * @param y Panel screen y.
 * @param width Filled width in pixels.
 */
static inline void field_hud_set_bar_geometry(POLY_G4* bar, s32 x, s32 y, s32 width)
{
    bar->x0 = g_field_hud_bar_offset_x + x;
    bar->y1 = g_field_hud_bar_offset_y + y;
    bar->x1 = (g_field_hud_bar_offset_x + x) + width;
    bar->x2 = (g_field_hud_bar_offset_x + x) - FIELD_HUD_BAR_SLANT;
    bar->y0 = g_field_hud_bar_offset_y + y;
    bar->y3 = bar->y1 + g_field_hud_bar_height;
    bar->y2 = bar->y3;
    bar->x3 = (g_field_hud_bar_offset_x + x) + width - FIELD_HUD_BAR_SLANT;
}

/**
 * @brief Set the slanted corners of an HP change quad from its x0/x1 and link it.
 * @param prim Quad whose x0, x1 and color are already set.
 * @param y Panel screen y, added to g_field_hud_bar_offset_y.
 * @param ot Ordering-table entry to link into.
 * @return Pointer just past the quad.
 */
static inline POLY_F4* field_hud_link_change_quad(POLY_F4* prim, s32 y, OT_TYPE* ot)
{
    prim->x3 = prim->x1 - FIELD_HUD_BAR_SLANT;
    prim->y1 = g_field_hud_bar_offset_y + y;
    prim->x2 = prim->x0 - FIELD_HUD_BAR_SLANT;
    prim->y0 = g_field_hud_bar_offset_y + y;
    prim->y3 = prim->y1 + g_field_hud_bar_height;
    prim->y2 = prim->y3;
    lom_gpu_link(ot, prim);
    return prim + 1;
}

/**
 * @brief Draw the party panels and the panels of recently hit enemies.
 * @param render_half Render half that receives the primitives.
 * @note Party members not performing an action get their special attack gauge shown.
 */
void field_draw_actor_hud(FieldRenderHalf* render_half)
{
    Vec2s position;
    s32 i;
    s32 panel_count;
    s16 panel_y;
    s32 boss_drawn;
    FieldActor* anchor;
    s32 group;
    u32 current_hp;
    u32 displayed_hp;
    s32 world_y;
    s32 projected_y;
    s32 coordinate;
    s32 camera_y;

    for (panel_count = i = 0; i < FIELD_PARTY_COUNT; i++)
    {
        if (g_field_actors[i].presence != FIELD_ACTOR_UNUSED && (g_field_player_records[i].head.bytes.flags & FIELD_PLAYER_ACTIVE))
        {
            if (g_field_actors[i].command != FIELD_ACTOR_COMMAND_ACTION && g_field_actors[i].command != FIELD_ACTOR_COMMAND_INSTRUMENT)
            {
                g_field_object_states[i].hud.word |= FIELD_HUD_SHOW_TECHNIQUE_GAUGE;
            }

            panel_count++;
        }
    }

    switch (panel_count)
    {
    case 1:
        for (i = 0; i < FIELD_PARTY_COUNT; i++)
        {
            if (g_field_actors[i].presence != FIELD_ACTOR_UNUSED && (g_field_player_records[i].head.bytes.flags & FIELD_PLAYER_ACTIVE))
            {
                field_draw_actor_hud_panel(FIELD_HUD_SINGLE_X, FIELD_HUD_PARTY_Y, i, render_half, FIELD_HUD_HP_PER_BAR);
            }
        }

        break;
    case 2:
        panel_count = 0;

        for (i = 0; i < FIELD_PARTY_COUNT; i++)
        {
            if (g_field_actors[i].presence != FIELD_ACTOR_UNUSED && (g_field_player_records[i].head.bytes.flags & FIELD_PLAYER_ACTIVE))
            {
                field_draw_actor_hud_panel(FIELD_HUD_PAIR_X + panel_count * FIELD_HUD_PAIR_STEP, FIELD_HUD_PARTY_Y, i, render_half, FIELD_HUD_HP_PER_BAR);
                panel_count++;
            }
        }

        break;
    case 3:
        panel_count = 0;

        for (i = 0; i < FIELD_PARTY_COUNT; i++)
        {
            if (g_field_actors[g_field_party_hud_order[i]].presence != FIELD_ACTOR_UNUSED &&
                (g_field_player_records[g_field_party_hud_order[i]].head.bytes.flags & FIELD_PLAYER_ACTIVE))
            {
                panel_y = FIELD_HUD_TRIO_LOW_Y;

                if (i & 1)
                {
                    panel_y = FIELD_HUD_TRIO_HIGH_Y;
                }

                field_draw_actor_hud_panel(FIELD_HUD_TRIO_X + panel_count * FIELD_HUD_TRIO_STEP, panel_y, g_field_party_hud_order[i], render_half,
                                           FIELD_HUD_HP_PER_BAR);
                panel_count++;
            }
        }

        break;
    }

    boss_drawn = 0;

    if (g_field_scene_mode_bit != 0)
    {
        if (g_field_battle_entry_state == 0)
        {
            for (i = FIELD_PARTY_COUNT; i < FIELD_ACTOR_COUNT; i++)
            {
                group = g_field_object_states[i].group.flags & FIELD_OBJECT_GROUP_MASK;

                if (group == g_field_active_group && group != 0)
                {
                    if (g_field_object_states[i].hp_display.bits.boss)
                    {
                        if (g_field_actors[i].presence != FIELD_ACTOR_UNUSED && g_field_object_states[i].hp_display.bits.value != 0 && boss_drawn == 0)
                        {
                            field_draw_actor_hud_panel(FIELD_HUD_BOSS_X, FIELD_HUD_BOSS_Y, i, render_half, FIELD_HUD_BOSS_HP_PER_BAR);
                            boss_drawn = 1;
                        }
                    }
                    else if (g_field_actors[i].presence != FIELD_ACTOR_UNUSED)
                    {
                        current_hp = g_field_object_states[i].current_hp.word;
                        displayed_hp = g_field_object_states[i].hp_display.bits.value;

                        if (current_hp < displayed_hp)
                        {
                            g_field_object_states[i].hp_display.bits.panel_timer = FIELD_HUD_PANEL_FRAMES;
                        }
                        else if (displayed_hp != current_hp)
                        {
                            g_field_object_states[i].hp_display.bits.panel_timer = FIELD_HUD_PANEL_FRAMES;
                        }

                        if (g_field_object_states[i].hp_display.bits.panel_timer != 0)
                        {
                            if (!(g_field_object_states[i].flags & FIELD_OBJECT_FLAG_FROZEN) &&
                                (g_field_object_states[i].contact.bytes.flags & FIELD_CONTACT_ANIMATION_HIDDEN) &&
                                (FIELD_EFFECT_ACTOR_RECORDS[g_field_object_states[i].linked_effect_index].animation & FIELD_ANIMATION_INDEX_MASK) !=
                                    FIELD_HUD_ANCHOR_IGNORED_ANIMATION)
                            {
                                anchor = &FIELD_EFFECT_ACTOR_RECORDS[g_field_object_states[i].linked_effect_index];
                                {
                                    s32 camera_x = g_field_view_offset_x / 256;
                                    s32 world_x = anchor->x / 256 + FIELD_SCREEN_CENTER_X;
                                    coordinate = camera_x + world_x;
                                    position.x = coordinate;
                                }

                                projected_y = g_field_view_offset_y / 256;
                                world_y = FIELD_EFFECT_ACTOR_RECORDS[g_field_object_states[i].linked_effect_index].y / 256 + FIELD_SCREEN_CENTER_Y;
                                projected_y = projected_y + world_y;
                                coordinate = projected_y - FIELD_EFFECT_ACTOR_RECORDS[g_field_object_states[i].linked_effect_index].z / 512 -
                                             g_field_view_offset_z / 512;
                                position.y = coordinate;

                                coordinate = position.x;

                                if (coordinate + FIELD_HUD_MARGIN_X > SCREEN_WIDTH)
                                {
                                    position.x = SCREEN_WIDTH - FIELD_HUD_MARGIN_X;
                                }

                                coordinate = position.x;

                                if (coordinate < FIELD_HUD_MARGIN_X)
                                {
                                    position.x = FIELD_HUD_MARGIN_X;
                                }

                                coordinate = position.y;

                                if (coordinate > FIELD_HUD_MAX_Y)
                                {
                                    position.y = FIELD_HUD_MAX_Y;
                                }

                                coordinate = position.y;

                                if (coordinate < FIELD_HUD_MIN_Y)
                                {
                                    position.y = FIELD_HUD_MIN_Y;
                                }
                            }
                            else
                            {
                                {
                                    s32 camera_x = g_field_view_offset_x / 256;
                                    s32 world_x = g_field_actors[i].x / 256 + FIELD_SCREEN_CENTER_X;
                                    coordinate = camera_x + world_x;
                                    position.x = coordinate;
                                }

                                camera_y = g_field_view_offset_y / 256;
                                {
                                    s32 actor_y = g_field_actors[i].y / 256 + FIELD_SCREEN_CENTER_Y;
                                    projected_y = camera_y + actor_y;
                                }

                                coordinate = projected_y - g_field_actors[i].z / 512 - g_field_view_offset_z / 512;
                                position.y = coordinate;

                                coordinate = position.x;

                                if (coordinate + FIELD_HUD_MARGIN_X > SCREEN_WIDTH)
                                {
                                    position.x = SCREEN_WIDTH - FIELD_HUD_MARGIN_X;
                                }

                                coordinate = position.x;

                                if (coordinate < FIELD_HUD_MARGIN_X)
                                {
                                    position.x = FIELD_HUD_MARGIN_X;
                                }

                                coordinate = position.y;

                                if (coordinate > FIELD_HUD_MAX_Y)
                                {
                                    position.y = FIELD_HUD_MAX_Y;
                                }

                                coordinate = position.y;

                                if (coordinate < FIELD_HUD_MIN_Y)
                                {
                                    position.y = FIELD_HUD_MIN_Y;
                                }
                            }

                            field_draw_actor_hud_panel(position.x - FIELD_HUD_ENEMY_OFFSET_X, position.y, i, render_half, FIELD_HUD_HP_PER_BAR);

                            if (g_field_object_states[i].hp_display.bits.panel_timer != 0)
                            {
                                g_field_object_states[i].hp_display.bits.panel_timer--;
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief Draw one HUD panel and move its displayed HP toward the current HP.
 * @param x Screen x of the panel.
 * @param y Screen y of the panel, before the shake offset.
 * @param slot Object index; party members (0 to 2) also get the blinking portrait and the HP percentage.
 * @param render_half Render half whose primitive cursor is advanced.
 * @param hp_per_bar HP of one gauge bar; each full bar is drawn in the next color.
 */
static void field_draw_actor_hud_panel(s32 x, s32 y, s32 slot, FieldRenderHalf* render_half, s32 hp_per_bar)
{
    Vec2s* scratch = FIELD_HUD_SCRATCH_POSITION;
    FieldRenderHalf* ctx;
    s32 recovery_width;
    s32 scan_slot;
    u32 boss_panel;
    s16 full_x;
    s16 full_y;
    s16 full_right;
    s32 partial_width;
    s16 rising_base_x;
    s16 falling_base_x;
    s16 rising_right;
    s16 falling_right;
    s16 blink_uv;
    u16 rising_x;
    u16 falling_x;
    s32 full_color;
    s32 partial_color;
    s32 palette_index;
    u16 intensity;
    u32 current_hp;
    u32 displayed_value;
    u32 partial_bar_count;
    u32 full_palette_level;
    u32 hp_or_bar_count;
    u32 rising_current;
    u32 rising_wrapped_current;
    u32 falling_current;
    u32 falling_wrapped_current;
    u32 rising_displayed;
    u32 falling_displayed;
    u32 maximum_hp;
    u32 current_value;
    u32 hp_percent;
    CVECTOR* colors;
    s32 background_type;
    u8 gauge_type;
    FieldObjectState* state;
    void* label_cursor;
    void* status_cursor;
    POLY_F4* change_quad;
    SPRT* sprite_cursor;
    POLY_G4* gauge_cursor;
    POLY_G4* bar;

    if (slot < FIELD_PARTY_COUNT)
    {
        if (g_field_player_records[slot].hit_state < FIELD_HUD_SHAKE_FRAMES)
        {
            y += g_field_hud_shake_offsets[g_field_player_records[slot].hit_state];

            if (g_field_player_records[slot].hit_state != 0)
            {
                g_field_player_records[slot].hit_state--;
            }
            else
            {
                g_field_player_records[slot].hit_state = FIELD_HUD_SHAKE_IDLE;
            }
        }
    }

    state = &g_field_object_states[slot];
    boss_panel = state->hp_display.bits.boss;

    if ((boss_panel != 0) && (g_field_boss_hud_shake_frame < FIELD_HUD_SHAKE_FRAMES))
    {
        y += g_field_hud_shake_offsets[g_field_boss_hud_shake_frame];

        if (g_field_boss_hud_shake_frame != 0)
        {
            g_field_boss_hud_shake_frame--;
        }
        else
        {
            g_field_boss_hud_shake_frame = FIELD_HUD_SHAKE_IDLE;
        }
    }

    ctx = render_half;
    label_cursor = ctx->primitive_cursor;

    if (state->hud.bytes.object_index < FIELD_PARTY_COUNT)
    {
        g_field_hud_bar_offset_x = FIELD_HUD_PARTY_BAR_X;
        g_field_hud_bar_offset_y = FIELD_HUD_PARTY_BAR_Y;
        scratch->x = x;
        scratch->y = y;
        label_cursor = field_emit_actor_portrait(label_cursor, &ctx->ordering_table[FIELD_HUD_OT_INDEX], state->hud.bytes.object_index, scratch);
        g_field_hud_bar_width = FIELD_HUD_BAR_WIDTH;
        g_field_hud_bar_height = FIELD_HUD_BAR_HEIGHT;
    }
    else if (boss_panel != 0)
    {
        g_field_hud_bar_offset_x = FIELD_HUD_BOSS_BAR_X;
        g_field_hud_bar_offset_y = FIELD_HUD_BOSS_BAR_Y;
        g_field_hud_bar_width = FIELD_HUD_BOSS_BAR_WIDTH;
        g_field_hud_bar_height = FIELD_HUD_BAR_HEIGHT;
    }
    else
    {
        g_field_hud_bar_offset_x = FIELD_HUD_ENEMY_BAR_X;
        g_field_hud_bar_offset_y = FIELD_HUD_ENEMY_BAR_Y;
        g_field_hud_bar_width = FIELD_HUD_BAR_WIDTH;
        g_field_hud_bar_height = FIELD_HUD_BAR_HEIGHT;
    }

    sprite_cursor = label_cursor;

    if (slot < FIELD_PARTY_COUNT)
    {
        if (g_field_hud_blink_frames[slot] >= FIELD_HUD_BLINK_FRAMES || ++g_field_hud_blink_frames[slot] >= FIELD_HUD_BLINK_FRAMES)
        {
            for (scan_slot = 0; scan_slot < FIELD_HUD_BLINK_LIST_COUNT; scan_slot++)
            {
                if (g_field_pair_indicator_list[scan_slot] == FIELD_HUD_BLINK_LIST_END)
                {
                    break;
                }

                if (g_field_pair_indicator_list[scan_slot] == slot)
                {
                    g_field_hud_blink_frames[slot] = 0;
                    break;
                }
            }

            if ((g_field_hud_blink_frames[slot] != 0) && !(rand() & FIELD_HUD_BLINK_CHANCE_MASK))
            {
                g_field_hud_blink_frames[slot] = 0;
            }
        }

        SET_BGR0_PACKED(sprite_cursor, GPU_TINT_NEUTRAL);
        lom_gpu_init_sprite(sprite_cursor);
        sprite_cursor->x0 = (x + FIELD_HUD_BLINK_X);
        sprite_cursor->y0 = (y + FIELD_HUD_BLINK_Y);

        if (g_field_hud_blink_frames[slot] < FIELD_HUD_BLINK_FRAMES)
        {
            blink_uv = ((u16)g_field_hud_blink_frames[slot] * FIELD_HUD_BLINK_SIZE) + FIELD_HUD_BLINK_UV;
        }
        else
        {
            blink_uv = FIELD_HUD_BLINK_UV;
        }

        SET_SPRT_WH_PACKED(sprite_cursor, FIELD_HUD_BLINK_SIZE, FIELD_HUD_BLINK_SIZE);
        SET_SPRT_UV0_PACKED(sprite_cursor, blink_uv);
        sprite_cursor->clut = FIELD_HUD_BLINK_CLUT;
        lom_gpu_link(&ctx->ordering_table[FIELD_HUD_OT_INDEX], sprite_cursor);
        sprite_cursor++;
        scratch->x = x + FIELD_HUD_PERCENT_X;
        scratch->y = y;
        current_hp = state->current_hp.word;
        maximum_hp = state->maximum_hp;
        hp_percent = current_hp * 100;

        if (maximum_hp != 0)
        {
            hp_percent = hp_percent / maximum_hp;
        }

        if ((hp_percent == 0) && (current_hp != 0))
        {
            hp_percent = 1;
        }

        sprite_cursor = field_emit_hud_percentage(sprite_cursor, ctx, hp_percent, scratch);
    }

    gauge_type = state->hud.bytes.object_index;
    status_cursor = sprite_cursor;

    if (gauge_type < FIELD_PLAYER_COUNT)
    {
        if (state->hud.word & FIELD_HUD_SHOW_TECHNIQUE_GAUGE)
        {
            if (state->technique_gauge == FIELD_TECHNIQUE_GAUGE_FULL)
            {
                if (g_frame_counter & 8)
                {
                    colors = g_field_hud_full_status_colors;
                }
                else
                {
                    colors = g_field_hud_status_colors;
                }
            }
            else
            {
                colors = g_field_hud_status_colors;
            }

            intensity = state->technique_gauge;
            status_cursor = field_emit_hud_status_line(status_cursor, ctx, colors, intensity, x, y);
        }
        else
        {
            colors = g_field_hud_effect_colors;
            intensity = state->action_charge;
            status_cursor = field_emit_hud_status_line(status_cursor, ctx, colors, intensity, x, y);
        }
    }
    else if ((gauge_type == FIELD_PARTY_COMPANION) && (g_field_player_records[FIELD_PARTY_COMPANION].character_id >= FIELD_COMPANION_COLOR_ID_MIN))
    {
        colors = g_field_hud_companion_status_colors;
        status_cursor = field_emit_hud_status_line(status_cursor, ctx, colors, state->technique_gauge, x, y);
    }

    gauge_cursor = status_cursor;

    if (slot < FIELD_PARTY_COUNT && g_field_player_records[slot].revive_delay != 0 && g_field_actors[slot].command == FIELD_ACTOR_COMMAND_KNOCKED_DOWN)
    {
        bar = gauge_cursor;
        SET_BGR0_PACKED(bar, FIELD_HUD_RECOVERY_DARK);
        SET_POLY_G4_BGR2_PACKED(bar, FIELD_HUD_RECOVERY_DARK);
        lom_gpu_init_poly_g4(bar);
        SET_POLY_G4_BGR1_PACKED(bar, GPU_COLOR_WHITE);
        SET_POLY_G4_BGR3_PACKED(bar, GPU_COLOR_WHITE);
        recovery_width = (g_field_hud_bar_width * g_field_player_records[slot].revive_time) / g_field_player_records[slot].revive_delay;
        field_hud_set_bar_geometry(bar, x, y, recovery_width);
        lom_gpu_link(&ctx->ordering_table[FIELD_HUD_OT_INDEX], bar);
        gauge_cursor++;
    }
    else
    {
        current_value = state->current_hp.word;

        if (current_value != 0)
        {
            partial_bar_count = current_value / hp_per_bar;
            palette_index = partial_bar_count & 3;

            if (partial_bar_count >= 3)
            {
                palette_index |= 2;
            }

            if ((s32)(state->hp_display.bits.value - current_value) < hp_per_bar)
            {
                bar = gauge_cursor;
                SET_BGR0_PACKED(bar, g_field_hud_hp_colors[palette_index] & FIELD_HUD_COLOR_DIM);
                SET_POLY_G4_BGR1_PACKED(bar, g_field_hud_hp_colors[palette_index] & FIELD_HUD_COLOR_MID);
                partial_color = g_field_hud_hp_colors[palette_index];
                lom_gpu_init_poly_g4(bar);
                SET_POLY_G4_BGR3_PACKED(bar, partial_color);
                SET_POLY_G4_BGR2_PACKED(bar, partial_color);
                partial_width = (s32)(g_field_hud_bar_width * (current_value % hp_per_bar)) / hp_per_bar;
                field_hud_set_bar_geometry(bar, x, y, partial_width);
                lom_gpu_link(&ctx->ordering_table[FIELD_HUD_OT_INDEX], bar);
                gauge_cursor++;
            }

            hp_or_bar_count = (u32)state->current_hp.word / hp_per_bar;
            full_palette_level = hp_or_bar_count - 1;

            if (hp_or_bar_count != 0)
            {
                palette_index = full_palette_level & 3;

                if (full_palette_level >= 3)
                {
                    palette_index |= 2;
                }

                bar = gauge_cursor;
                SET_BGR0_PACKED(bar, g_field_hud_hp_colors[palette_index] & FIELD_HUD_COLOR_DIM);
                SET_POLY_G4_BGR2_PACKED(bar, g_field_hud_hp_colors[palette_index] & FIELD_HUD_COLOR_MID);
                lom_gpu_init_poly_g4(bar);
                full_color = g_field_hud_hp_colors[palette_index];
                SET_POLY_G4_BGR3_PACKED(bar, full_color);
                SET_POLY_G4_BGR1_PACKED(bar, full_color);
                full_x = g_field_hud_bar_offset_x;
                full_right = g_field_hud_bar_width;
                full_y = g_field_hud_bar_offset_y;
                full_x += x;
                full_right += full_x;
                bar->x1 = full_right;
                full_right -= FIELD_HUD_BAR_SLANT;
                full_y += y;
                bar->x3 = full_right;
                bar->y1 = full_y;
                bar->x0 = full_x;
                full_x -= FIELD_HUD_BAR_SLANT;
                bar->x2 = full_x;
                bar->y0 = full_y;
                bar->y3 = bar->y1 + g_field_hud_bar_height;
                bar->y2 = bar->y3;
                lom_gpu_link(&ctx->ordering_table[FIELD_HUD_OT_INDEX], bar);
                gauge_cursor++;
            }
        }
    }

    hp_or_bar_count = state->current_hp.word;
    displayed_value = state->hp_display.bits.value;

    if (displayed_value < hp_or_bar_count)
    {
        if (hp_or_bar_count - displayed_value >= FIELD_HUD_RISE_MIN_STEP)
        {
            state->hp_display.bits.value = displayed_value + ((hp_or_bar_count - displayed_value) / 3);
        }
        else
        {
            state->hp_display.bits.value = displayed_value + 1;
        }

        rising_current = state->current_hp.word;

        if ((rising_current / hp_per_bar) == (state->hp_display.bits.value / hp_per_bar))
        {
            rising_x = g_field_hud_bar_offset_x;
            change_quad = (POLY_F4*)gauge_cursor;
            rising_x += x;
            change_quad->x1 = (rising_x + ((g_field_hud_bar_width * (rising_current % hp_per_bar)) / hp_per_bar));
            rising_x += ((g_field_hud_bar_width * (state->hp_display.bits.value % hp_per_bar)) / hp_per_bar);
            change_quad->x0 = rising_x;
            status_cursor = field_emit_hud_healing_quad(change_quad, y, &ctx->ordering_table[FIELD_HUD_DELTA_OT_INDEX]);
        }
        else
        {
            change_quad = (POLY_F4*)gauge_cursor;
            rising_base_x = g_field_hud_bar_offset_x + x;
            change_quad->x1 = rising_base_x;
            change_quad->x0 = (rising_base_x + ((g_field_hud_bar_width * (state->hp_display.bits.value % hp_per_bar)) / hp_per_bar));
            change_quad = field_emit_hud_healing_quad(change_quad, y, &ctx->ordering_table[FIELD_HUD_DELTA_OT_INDEX]);
            rising_displayed = state->hp_display.bits.value;
            rising_wrapped_current = state->current_hp.word;

            if (rising_displayed - rising_wrapped_current < hp_per_bar)
            {
                rising_right = g_field_hud_bar_offset_x + x + ((g_field_hud_bar_width * (rising_wrapped_current % hp_per_bar)) / hp_per_bar);
                change_quad->x1 = rising_right;
            }
            else
            {
                rising_right = g_field_hud_bar_offset_x + x;
                change_quad->x1 = rising_right;
            }

            change_quad->x0 = ((g_field_hud_bar_offset_x + x) + g_field_hud_bar_width);
            status_cursor = field_emit_hud_healing_quad(change_quad, y, &ctx->ordering_table[FIELD_HUD_DELTA_OT_INDEX]);
        }

        gauge_cursor = status_cursor;
    }
    else if (hp_or_bar_count < displayed_value)
    {
        if (displayed_value - hp_or_bar_count >= FIELD_HUD_FALL_MIN_STEP)
        {
            state->hp_display.bits.value = displayed_value - ((displayed_value - hp_or_bar_count) / 3);
        }
        else
        {
            state->hp_display.bits.value = displayed_value - 1;
        }

        falling_current = state->current_hp.word;

        if ((falling_current / hp_per_bar) == (state->hp_display.bits.value / hp_per_bar))
        {
            falling_x = g_field_hud_bar_offset_x;
            change_quad = (POLY_F4*)gauge_cursor;
            falling_x += x;
            change_quad->x1 = (falling_x + ((g_field_hud_bar_width * (falling_current % hp_per_bar)) / hp_per_bar));
            falling_x += ((g_field_hud_bar_width * (state->hp_display.bits.value % hp_per_bar)) / hp_per_bar);
            change_quad->x0 = falling_x;
            status_cursor = field_emit_hud_damage_quad(change_quad, y, &ctx->ordering_table[FIELD_HUD_DELTA_OT_INDEX]);
        }
        else
        {
            change_quad = (POLY_F4*)gauge_cursor;
            falling_base_x = g_field_hud_bar_offset_x + x;
            change_quad->x1 = falling_base_x;
            change_quad->x0 = (falling_base_x + ((g_field_hud_bar_width * (state->hp_display.bits.value % hp_per_bar)) / hp_per_bar));
            change_quad = field_emit_hud_damage_quad(change_quad, y, &ctx->ordering_table[FIELD_HUD_DELTA_OT_INDEX]);
            falling_displayed = state->hp_display.bits.value;
            falling_wrapped_current = state->current_hp.word;

            if (falling_displayed - falling_wrapped_current < hp_per_bar)
            {
                falling_right = g_field_hud_bar_offset_x + x + ((g_field_hud_bar_width * (falling_wrapped_current % hp_per_bar)) / hp_per_bar);
                change_quad->x1 = falling_right;
            }
            else
            {
                falling_right = g_field_hud_bar_offset_x + x;
                change_quad->x1 = falling_right;
            }

            change_quad->x0 = ((g_field_hud_bar_offset_x + x) + g_field_hud_bar_width);
            status_cursor = field_emit_hud_damage_quad(change_quad, y, &ctx->ordering_table[FIELD_HUD_DELTA_OT_INDEX]);
        }

        gauge_cursor = status_cursor;
    }

    sprite_cursor = (SPRT*)gauge_cursor;
    SET_BGR0_PACKED(sprite_cursor, GPU_TINT_NEUTRAL);
    lom_gpu_init_sprite(sprite_cursor);
    lom_gpu_set_semi_trans(sprite_cursor, 1);
    SET_SPRT_XY0_WORD(sprite_cursor, (y << 16) + x);
    background_type = state->hud.bytes.object_index;

    switch (background_type)
    {
    case 0:
    case 1:
        SET_SPRT_WH_PACKED(sprite_cursor, FIELD_HUD_PANEL_WIDTH, FIELD_HUD_PANEL_HEIGHT);
        SET_SPRT_UV0_PACKED(sprite_cursor, FIELD_HUD_PANEL_UV);
        break;
    case 2:
        if (g_field_player_records[FIELD_PARTY_COMPANION].character_id >= FIELD_COMPANION_COLOR_ID_MIN)
        {
            SET_SPRT_WH_PACKED(sprite_cursor, FIELD_HUD_PANEL_WIDTH, FIELD_HUD_PANEL_HEIGHT);
            SET_SPRT_UV0_PACKED(sprite_cursor, FIELD_HUD_PANEL_UV);
        }
        else
        {
            SET_SPRT_WH_PACKED(sprite_cursor, FIELD_HUD_PANEL_WIDTH, FIELD_HUD_PANEL_HEIGHT);
            SET_SPRT_UV0_PACKED(sprite_cursor, FIELD_HUD_COMPANION_PANEL_UV);
        }

        break;
    default:
        if (boss_panel != 0)
        {
            SET_SPRT_WH_PACKED(sprite_cursor, FIELD_HUD_BOSS_PANEL_WIDTH, FIELD_HUD_BOSS_PANEL_HEIGHT);
            SET_SPRT_UV0_PACKED(sprite_cursor, FIELD_HUD_BOSS_PANEL_UV);
        }
        else
        {
            SET_SPRT_WH_PACKED(sprite_cursor, FIELD_HUD_ENEMY_PANEL_WIDTH, FIELD_HUD_ENEMY_PANEL_HEIGHT);
            SET_SPRT_UV0_PACKED(sprite_cursor, FIELD_HUD_ENEMY_PANEL_UV);
        }

        break;
    }

    sprite_cursor->clut = FIELD_HUD_CLUT;
    lom_gpu_link(&ctx->ordering_table[FIELD_HUD_OT_INDEX], sprite_cursor);
    sprite_cursor++;
    lom_gpu_init_tpage_packet(sprite_cursor, 0, 0, FIELD_UI_TPAGE(GPU_BLEND_HALF));
    lom_gpu_link(&ctx->ordering_table[FIELD_HUD_OT_INDEX], sprite_cursor);
    render_half->primitive_cursor = (u8*)sprite_cursor + sizeof(DR_TPAGE);
}

/**
 * @brief Set one end-color channel of a gauge line.
 * @param component Channel byte of the line's second vertex.
 * @param empty Channel value of the empty-gauge color.
 * @param full Channel value of the full-gauge color.
 * @param intensity Gauge value, 0 to 255.
 * @note A channel the two colors share keeps its value; otherwise it rises with the gauge (full color set) or falls with it.
 */
static inline void field_hud_set_faded_component(u8* component, u8 empty, u8 full, s32 intensity)
{
    if (empty == full)
    {
        *component = empty;
    }
    else if (full != 0)
    {
        *component = intensity;
    }
    else
    {
        *component = ~intensity;
    }
}

/**
 * @brief Add the HUD status line, a gradient whose length and end color follow an intensity.
 * @param line Free primitive space for the line.
 * @param render_half Render half whose ordering table receives the line.
 * @param colors Start color, then the end color at full intensity.
 * @param intensity 0 to 255; 0 draws nothing.
 * @param x Panel x position.
 * @param y Panel y position.
 * @return The next free primitive space.
 */
static u8* field_emit_hud_status_line(LINE_G2* line, FieldRenderHalf* render_half, CVECTOR* colors, s32 intensity, s32 x, s32 y)
{
    s16 line_x;
    s16 line_y;
    u32 first_color;

    if (intensity == 0)
    {
        return (u8*)line;
    }

    first_color = *(u32*)&colors[0];
    lom_gpu_set_length(line, 4);
    SET_BGR0_PACKED(line, first_color);
    lom_gpu_set_code(line, GPU_CODE_LINE_G2);

    field_hud_set_faded_component(&line->r1, colors[0].r, colors[1].r, intensity);

    field_hud_set_faded_component(&line->g1, colors[0].g, colors[1].g, intensity);

    field_hud_set_faded_component(&line->b1, colors[0].b, colors[1].b, intensity);

    line_x = x + FIELD_HUD_LINE_X;
    line->x0 = line_x;
    line_y = y + FIELD_HUD_LINE_Y;
    line->y1 = line_y;
    line->y0 = line_y;
    line->x1 = line_x + ((intensity * FIELD_HUD_LINE_LENGTH) / 255);

    lom_gpu_link(&render_half->ordering_table[FIELD_HUD_OT_INDEX], line);
    return (u8*)(line + 1);
}

/**
 * @brief Emit a right-aligned HP percentage followed by the percent glyph.
 * @param packet Primitive-buffer cursor.
 * @param render_half Render half forwarded to field_emit_hud_glyph.
 * @param value HP percentage, 0 to 100.
 * @param position Screen position; x advances 7 pixels per column.
 * @return Pointer just past the percent glyph.
 * @note Leading zeros in the hundreds and tens columns are left blank.
 */
static void* field_emit_hud_percentage(void* packet, FieldRenderHalf* render_half, s32 value, Vec2s* position)
{
    s32 emitted;

    emitted = 0;

    if (value / 100 != 0)
    {
        emitted = 1;
        packet = field_emit_hud_glyph(packet, render_half, value / 100, position);
        value -= 100;
    }

    position->x += FIELD_HUD_GLYPH_ADVANCE;

    if (emitted || value / 10 != 0)
    {
        s32 digit;
        digit = value / 10;
        packet = field_emit_hud_glyph(packet, render_half, digit, position);
        value -= digit * 10;
    }

    position->x += FIELD_HUD_GLYPH_ADVANCE;
    packet = field_emit_hud_glyph(packet, render_half, value, position);
    position->x += FIELD_HUD_GLYPH_ADVANCE;
    return field_emit_hud_glyph(packet, render_half, FIELD_HUD_GLYPH_PERCENT, position);
}

/**
 * @brief Emit a digit or percent glyph from the HUD texture strip.
 * @param sprite Sprite to fill.
 * @param render_half Render half whose HUD ordering-table entry receives the sprite.
 * @param value Glyph index: 0 to 9 for digits, 10 for percent.
 * @param position Packed screen x/y.
 * @return Pointer just past the sprite.
 */
static void* field_emit_hud_glyph(SPRT* sprite, FieldRenderHalf* render_half, s32 value, Vec2s* position)
{
    s32 packed_xy;

    SET_BGR0_PACKED(sprite, GPU_TINT_NEUTRAL);
    lom_gpu_init_sprite(sprite);
    packed_xy = *(s32*)position;
    SET_SPRT_UV0_PACKED(sprite, (value * FIELD_HUD_GLYPH_WIDTH) + FIELD_HUD_GLYPH_UV);
    SET_SPRT_WH_PACKED(sprite, FIELD_HUD_GLYPH_WIDTH, FIELD_HUD_GLYPH_HEIGHT);
    sprite->clut = FIELD_HUD_CLUT;
    SET_SPRT_XY0_WORD(sprite, packed_xy);
    lom_gpu_link(&render_half->ordering_table[FIELD_HUD_OT_INDEX], sprite);
    return sprite + 1;
}

/**
 * @brief Finish and link a red damage quad whose x0 and x1 are already set.
 * @param prim Quad; the lower edge is slanted 3 pixels to the left.
 * @param y Panel screen y, added to g_field_hud_bar_offset_y.
 * @param ot Ordering-table entry to link into.
 * @return Pointer just past the quad.
 */
static POLY_F4* field_emit_hud_damage_quad(POLY_F4* prim, s32 y, OT_TYPE* ot)
{
    SET_BGR0_PACKED(prim, FIELD_HUD_RED);
    lom_gpu_init_poly_f4(prim);
    return field_hud_link_change_quad(prim, y, ot);
}

/**
 * @brief Finish and link a white healing quad, widening a zero-width quad to one pixel.
 * @param prim Quad whose x0 and x1 are already set.
 * @param y Panel screen y, added to g_field_hud_bar_offset_y.
 * @param ot Ordering-table entry to link into.
 * @return Pointer just past the quad.
 */
static POLY_F4* field_emit_hud_healing_quad(POLY_F4* prim, s32 y, OT_TYPE* ot)
{
    if (prim->x1 == prim->x0)
    {
        prim->x1 = prim->x0 + 1;
    }

    SET_BGR0_PACKED(prim, GPU_COLOR_WHITE);
    lom_gpu_init_poly_f4(prim);
    return field_hud_link_change_quad(prim, y, ot);
}

/**
 * @brief Emit a party member's portrait and its texture page.
 * @param sprt Sprite primitive to fill.
 * @param ot Ordering-table entry the sprite and its DR_TPAGE are linked into.
 * @param index Party slot; the companion slot uses the companion palette from FIELD_COMPANION_COLOR_ID_MIN on.
 * @param position Screen position of the portrait.
 * @return Pointer just past the DR_TPAGE.
 */
void* field_emit_actor_portrait(SPRT* sprt, OT_TYPE* ot, s32 index, Vec2s* position)
{
    DR_TPAGE* mode;

    SET_BGR0_PACKED(sprt, GPU_TINT_NEUTRAL);
    lom_gpu_init_sprite(sprt);
    SET_SPRT_XY0_WORD(sprt, *(u32*)position);
    SET_SPRT_UV0_PACKED(sprt, FIELD_HUD_PORTRAIT_UV);
    SET_SPRT_WH_PACKED(sprt, FIELD_HUD_PORTRAIT_SIZE, FIELD_HUD_PORTRAIT_SIZE);

    if (index == FIELD_PARTY_COMPANION && g_field_player_records[FIELD_PARTY_COMPANION].character_id >= FIELD_COMPANION_COLOR_ID_MIN)
    {
        sprt->clut = lom_gpu_clut(
            FIELD_UI_CLUT_X, (g_field_actors[FIELD_PARTY_COMPANION].control.half[1] & (FIELD_CONTROL_GOLEM_COLOR_MASK >> FIELD_CONTROL_GOLEM_COLOR_SHIFT)) +
                                 FIELD_HUD_COMPANION_CLUT_Y);
    }
    else
    {
        sprt->clut = ((index + FIELD_RESOURCE_CLUT_VRAM_Y) << 6) | FIELD_CONTROL_PALETTE(g_field_actors[index].control.word);
    }

    lom_gpu_link(ot, sprt);

    mode = (DR_TPAGE*)(sprt + 1);

    if (index >= FIELD_PARTY_COMPANION)
    {
        lom_gpu_init_tpage_packet(mode, 0, 0,
                                  lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_ADD, FIELD_COMPANION_TEXTURE_VRAM_X - index * FIELD_TEXTURE_COLUMN_WIDTH, 0));
    }
    else
    {
        lom_gpu_init_tpage_packet(mode, 0, 0,
                                  lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_ADD, FIELD_PARTY_TEXTURE_VRAM_X - index * FIELD_PARTY_TEXTURE_VRAM_STEP, 0));
    }

    lom_gpu_link(ot, mode);
    return mode + 1;
}

/**
 * @brief Read an image resource from disc into the field CD buffer and upload it to VRAM.
 * @param id Resource index; only the low 16 bits are used.
 * @param rect Pixel and CLUT destinations, see field_upload_image_resource.
 * @param mode CLUT upload mode, see field_upload_image_resource.
 * @return Result of field_upload_image_resource.
 */
s32 field_load_vram_resource(s32 id, RECT* rect, s32 mode)
{
    u8* buf = g_field_cd_buffer;

    cdrom_queue_read(id & 0xFFFF, buf);
    cdrom_wait_queue_empty();
    return field_upload_image_resource(rect, (Tim*)buf, mode);
}

/**
 * @brief Upload a TIM's CLUT block and pixel block into VRAM.
 * @param rect x/y: pixel destination, w/h: CLUT destination; returns the pixel width and height in x/y.
 * @param resource TIM file with a CLUT block followed by a pixel block.
 * @param mode Nonzero uploads the CLUT as one row of width * height entries; zero keeps its shape.
 * @return CLUT entries 240 and 241 as one packed word.
 * @note CLOAD calls it too, for its icon resources.
 */
s32 field_upload_image_resource(RECT* rect, Tim* resource, s32 mode)
{
    RECT load_rect;
    s32 offset;
    s32 palette_word;
    TimBlock* image;
    TimDimensions* dimensions;
    u16 x;

    dimensions = &resource->clut_block.dimensions;
    offset = resource->clut_block.bnum;

    if (mode != 0)
    {
        lom_gpu_set_rect(&load_rect, rect->w, rect->h, dimensions->width * dimensions->height, 1);
    }
    else
    {
        load_rect.x = rect->w;
        load_rect.y = rect->h;
        load_rect.w = dimensions->width;
        load_rect.h = dimensions->height;
    }

    lom_graphics_upload(&load_rect, (u_long*)resource->clut_data);

    image = TIM_PIXEL_BLOCK(resource, offset);
    x = rect->x;
    palette_word = *(s32*)&resource->clut_data[240];
    load_rect.x = x;
    load_rect.y = rect->y;
    dimensions = &image->dimensions;
    load_rect.w = dimensions->width;
    load_rect.h = dimensions->height;
    lom_graphics_upload(&load_rect, (u_long*)(image + 1));
    rect->x = dimensions->width;
    rect->y = dimensions->height;
    return palette_word;
}

/**
 * @brief Run the highest-priority available handler and acknowledge its object flag.
 * @param index Object index.
 * @param state Object flags and last acknowledged flag state.
 * @param pending_flags Flags that are active or have changed.
 */
static inline void field_dispatch_object_flag(s32 index, FieldObjectState* state, s32 pending_flags)
{
    s32 bit_mask;
    s32 bit_index;
    FieldObjectFlagHandler handler;

    bit_mask = 1 << (FIELD_OBJECT_HANDLER_COUNT - 1);

    for (bit_index = FIELD_OBJECT_HANDLER_COUNT - 1; bit_index >= 0; bit_index--, bit_mask >>= 1)
    {
        if (pending_flags & bit_mask)
        {
            handler = g_field_object_flag_handlers[bit_index];

            if (handler.animation != FIELD_OBJECT_HANDLER_NONE)
            {
                if (handler.animation < FIELD_OBJECT_HANDLER_FUNCTION_MIN)
                {
                    if (state->flags & bit_mask)
                    {
                        s32 animation_slot;

                        animation_slot = index + FIELD_OBJECT_EFFECT_SLOT_BASE;
                        field_start_builtin_animation(index, animation_slot, handler.animation);
                        field_start_actor_animation(animation_slot, 0, 0);
                    }
                }
                else
                {
                    handler.function(&g_field_actors[index], state->flags & bit_mask);
                }

                state->previous_flags = (state->previous_flags & ~bit_mask) | (state->flags & bit_mask);
                return;
            }
        }
    }
}

/**
 * @brief Dispatch object flag effects, stop cleared effects, and refresh actor tint.
 * @param index Object index.
 * @note A handler entry below 0x100 is an animation resource played on the object's effect slot.
 */
void field_update_object_effects(s32 index)
{
    s32 flags_before;
    s32 changed_or_current;
    s32 previous_flags;
    u32 changed_flags;
    s32 flags_current;
    s32 old_flags;
    s32 scan_flags;
    s32 status_bit;
    s32 cleared_bit;
    s32 cleared_mask;
    u16 animation_id;
    FieldActorSlot* slot;
    FieldActor* actor;
    FieldObjectState* state;

    actor = &g_field_actors[index];
    state = &g_field_object_states[index];
    flags_before = state->flags;
    slot = &g_field_actor_slots[FIELD_OBJECT_EFFECT_SLOT_BASE + index];

    if (flags_before & FIELD_OBJECT_IMMOBILE_FLAGS)
    {
        state->flags &= ~FIELD_OBJECT_FLAG_DASH;
        state->flags &= ~FIELD_OBJECT_CHAINING;
    }

    flags_current = state->flags;
    previous_flags = state->previous_flags;
    changed_or_current = (flags_current ^ previous_flags) | flags_current;

    if (changed_or_current != 0)
    {
        if (slot->active == 0)
        {
            field_dispatch_object_flag(index, state, changed_or_current);
        }
        else
        {
            if (!(flags_current & FIELD_OBJECT_CHAINING) && (previous_flags & FIELD_OBJECT_CHAINING))
            {
                slot->active = 0;
                slot->track_mask = 0;
                field_clear_actor_effects(slot);
                state->previous_flags &= ~FIELD_OBJECT_CHAINING;
            }
            else if (!(state->flags & FIELD_OBJECT_FLAG_DASH) && (state->previous_flags & FIELD_OBJECT_FLAG_DASH))
            {
                slot->active = 0;
                slot->track_mask = 0;
                field_clear_actor_effects(slot);
                state->previous_flags &= ~FIELD_OBJECT_FLAG_DASH;
            }
            else
            {
                changed_flags = state->flags;
                old_flags = state->previous_flags;
                changed_flags = (changed_flags ^ old_flags) & old_flags;

                if (changed_flags != 0)
                {
                    scan_flags = changed_flags;
                    cleared_bit = FIELD_OBJECT_HANDLER_COUNT - 1;

                    while (1)
                    {
                        cleared_mask = 1 << cleared_bit;

                        if (scan_flags & cleared_mask)
                        {
                            break;
                        }

                        cleared_bit--;
                    }

                    animation_id = slot->status.parts.animation_id;
                    status_bit = FIELD_STATUS_BIT_NONE;

                    switch (animation_id)
                    {
                    case FIELD_STATUS_ANIMATION_POISON:
                        status_bit = 0;
                        break;
                    case FIELD_STATUS_ANIMATION_DARKNESS:
                        status_bit = 1;
                        break;
                    case FIELD_STATUS_ANIMATION_PARALYSIS:
                        status_bit = 2;
                        break;
                    case FIELD_STATUS_ANIMATION_CONFUSION:
                        status_bit = 3;
                        break;
                    case FIELD_STATUS_ANIMATION_FLAME_BURST:
                        status_bit = 4;
                        break;
                    case FIELD_STATUS_ANIMATION_HYPNOSIS:
                        status_bit = 5;
                        break;
                    case FIELD_STATUS_ANIMATION_SLEEP:
                        status_bit = 6;
                        break;
                    case FIELD_STATUS_ANIMATION_PETRIFIED:
                        status_bit = 7;
                        break;
                    }

                    if (cleared_bit == status_bit)
                    {
                        slot->active = 0;
                        slot->track_mask = 0;
                        field_clear_actor_effects(slot);

                        if ((state->contact.word & FIELD_CONTACT_ANIMATION_HIDDEN) &&
                            (state->contact.bytes.controller_index == (actor->object_index + FIELD_OBJECT_EFFECT_SLOT_BASE)))
                        {
                            actor->presence = FIELD_ACTOR_VISIBLE;
                            state->contact.word &= ~FIELD_CONTACT_ANIMATION_HIDDEN;
                        }
                    }
                }
            }
        }
    }

    if (state->flags & FIELD_OBJECT_FLAG_HIT_FLASH)
    {
        g_field_actors[index].tint_blue = FIELD_HIT_FLASH_TINT;
        g_field_actors[index].tint_green = FIELD_HIT_FLASH_TINT;
        state->flags &= ~FIELD_OBJECT_FLAG_HIT_FLASH;
    }
    else
    {
        g_field_actors[index].tint_red = g_field_object_parts[index].tint_red;
        g_field_actors[index].tint_green = g_field_object_parts[index].tint_green;
        g_field_actors[index].tint_blue = g_field_object_parts[index].tint_blue;
    }
}

/**
 * @brief Paralysis flag handler: restart the actor's animation and play the paralysis animation.
 * @param actor Actor of the object.
 * @param is_set Nonzero when the flag became set; zero does nothing.
 * @note JP does not clear the linked object's FIELD_OBJECT_FLAG_LINK_TARGET.
 */
void field_handle_object_paralysis(FieldActor* actor, s32 is_set)
{
    if (is_set != 0)
    {
        actor->animation_state = 1;
        actor->animation_frame = 0;
        actor->animation &= FIELD_ANIMATION_FACING;
        actor->animation_active = 1;
        g_field_object_states[actor->object_index].movement.word &= ~FIELD_MOVEMENT_SEQUENCE_MASK;
        field_restart_actor_animation(actor);
        START_OBJECT_EFFECT(actor, FIELD_STATUS_ANIMATION_PARALYSIS);
#if !defined(VERSION_JP)
        field_clear_link_target_flag(actor->object_index);
#endif
    }
}

/**
 * @brief Hypnosis flag handler: restart the actor's animation and play the hypnosis animation.
 * @param actor Actor of the object.
 * @param is_set Nonzero when the flag became set; zero does nothing.
 */
void field_handle_object_hypnosis(FieldActor* actor, s32 is_set)
{
    if (is_set != 0)
    {
        actor->animation_state = 1;
        actor->animation_frame = 0;
        actor->animation &= FIELD_ANIMATION_FACING;
        actor->animation_active = 1;
        g_field_object_states[actor->object_index].movement.word &= ~FIELD_MOVEMENT_SEQUENCE_MASK;
        field_restart_actor_animation(actor);
        START_OBJECT_EFFECT(actor, FIELD_STATUS_ANIMATION_HYPNOSIS);
    }
}

/**
 * @brief Mini flag handler: shrink the actor's model to half size, or restore it.
 * @param actor Actor whose object part is updated.
 * @param is_set Nonzero when Mini became set (half size), zero when it was cleared (full size).
 */
void field_handle_object_mini(FieldActor* actor, s32 is_set)
{
    if (is_set != 0)
    {
        g_field_object_parts[actor->object_index].appearance.fields.scale_xz = FIELD_PART_SCALE_HALF;
        g_field_object_parts[actor->object_index].scale_y = FIELD_PART_SCALE_HALF;
    }
    else
    {
        g_field_object_parts[actor->object_index].appearance.fields.scale_xz = FIELD_PART_FULL_SCALE;
        g_field_object_parts[actor->object_index].scale_y = FIELD_PART_FULL_SCALE;
    }
}

/**
 * @brief Sleep flag handler: play the sleep animation and hold the actor in FIELD_ANIMATION_SLEEP_POSE.
 * @param actor Actor of the object.
 * @param is_set Nonzero when the flag became set; zero does nothing.
 * @note JP does not clear the linked object's FIELD_OBJECT_FLAG_LINK_TARGET.
 */
void field_handle_object_sleep(FieldActor* actor, s32 is_set)
{
    u8 animation;

    if (is_set != 0)
    {
        START_OBJECT_EFFECT(actor, FIELD_STATUS_ANIMATION_SLEEP);
        animation = actor->animation;

        if ((animation & FIELD_ANIMATION_INDEX_MASK) != FIELD_ANIMATION_SLEEP_POSE)
        {
            actor->animation = (animation & FIELD_ANIMATION_FACING) + FIELD_ANIMATION_SLEEP_POSE;
            actor->animation_state = 1;
            actor->animation_frame = 0;
            actor->animation_active = 1;
            g_field_object_states[actor->object_index].movement.word &= ~FIELD_MOVEMENT_SEQUENCE_MASK;
            field_restart_actor_animation(actor);
            actor->control.word |= FIELD_CONTROL_PLAY_ONCE;
        }
#if !defined(VERSION_JP)
        field_clear_link_target_flag(actor->object_index);
#endif
    }
}

/**
 * @brief Petrification flag handler: hold the actor in FIELD_ANIMATION_PETRIFIED_POSE, or release it and play effect resource 0x91.
 * @param actor Actor of the object.
 * @param is_set Nonzero when the flag became set, zero when it was cleared.
 * @note JP does not clear the linked object's FIELD_OBJECT_FLAG_LINK_TARGET.
 */
void field_handle_object_petrification(FieldActor* actor, s32 is_set)
{
    if (is_set != 0)
    {
        actor->animation_state = 1;
        actor->animation_active = 1;
        actor->animation_frame = 0;
        actor->animation = (actor->animation & FIELD_ANIMATION_FACING) + FIELD_ANIMATION_PETRIFIED_POSE;
        g_field_object_states[actor->object_index].movement.word &= ~FIELD_MOVEMENT_SEQUENCE_MASK;
        field_restart_actor_animation(actor);
        actor->control.word |= FIELD_CONTROL_PETRIFIED | FIELD_CONTROL_PLAY_ONCE;
#if !defined(VERSION_JP)
        field_clear_link_target_flag(actor->object_index);
#endif
    }
    else
    {
        actor->animation_frame = 0;
        actor->animation_state = 1;
        actor->animation_active = 1;
        actor->animation &= FIELD_ANIMATION_FACING;
        g_field_object_states[actor->object_index].movement.word &= ~FIELD_MOVEMENT_SEQUENCE_MASK;
        field_restart_actor_animation(actor);
        START_OBJECT_EFFECT(actor, 0x91);
        actor->control.word &= ~FIELD_CONTROL_PETRIFIED;
    }
}

/**
 * @brief Clear FIELD_OBJECT_FLAG_LINK_TARGET of the object that @p object_index is linked to, if it is linked.
 * @param object_index Object index.
 */
static inline void field_clear_link_target_flag(s32 object_index)
{
    FieldObjectState* states = g_field_object_states;
    FieldObjectState* state = &states[object_index];

    if ((state->contact.word >> FIELD_CONTACT_LINKED_SHIFT) & 1)
    {
        FieldObjectState* target = &states[state->linked_object_index];
        target->flags &= ~FIELD_OBJECT_FLAG_LINK_TARGET;
    }
}

/**
 * @brief Freeze flag handler: play the petrification animation and hide the actor, or play effect resource 13 and show it.
 * @param actor Actor of the object.
 * @param is_set Nonzero when the flag became set, zero when it was cleared.
 * @note JP does not clear the linked object's FIELD_OBJECT_FLAG_LINK_TARGET.
 */
void field_handle_object_freeze(FieldActor* actor, s32 is_set)
{
    if (is_set != 0)
    {
        START_OBJECT_EFFECT(actor, FIELD_STATUS_ANIMATION_PETRIFIED);
        actor->presence = FIELD_ACTOR_HIDDEN;
#if !defined(VERSION_JP)
        field_clear_link_target_flag(actor->object_index);
#endif
    }
    else
    {
        START_OBJECT_EFFECT(actor, 13);
        actor->presence = FIELD_ACTOR_VISIBLE;
    }
}

/**
 * @brief Knocked-out flag handler: clear FIELD_OBJECT_FLAG_LINK_TARGET of the object the actor is linked to.
 * @param actor Actor of the object.
 * @note JP's handler is empty.
 */
void field_handle_object_knocked_out(FieldActor* actor)
{
#if !defined(VERSION_JP)
    field_clear_link_target_flag(actor->object_index);
#endif
}

/**
 * @brief Action-chain flag handler (FIELD_OBJECT_CHAINING): play effect resource 14, or stop the object's effect slot.
 * @param actor Actor of the object.
 * @param is_set Nonzero when the flag became set, zero when it was cleared.
 */
void field_handle_object_chaining(FieldActor* actor, s32 is_set)
{
    if (is_set != 0)
    {
        START_OBJECT_EFFECT(actor, 14);
    }
    else
    {
        g_field_actor_slots[actor->object_index + FIELD_OBJECT_EFFECT_SLOT_BASE].active = 0;
        g_field_actor_slots[actor->object_index + FIELD_OBJECT_EFFECT_SLOT_BASE].track_mask = 0;
    }
}

/**
 * @brief Dash flag handler (FIELD_OBJECT_FLAG_DASH): play effect resource 25, or stop the object's effect slot.
 * @param actor Actor of the object.
 * @param is_set Nonzero when the flag became set, zero when it was cleared.
 */
void field_handle_object_dash(FieldActor* actor, s32 is_set)
{
    if (is_set != 0)
    {
        START_OBJECT_EFFECT(actor, 25);
    }
    else
    {
        g_field_actor_slots[actor->object_index + FIELD_OBJECT_EFFECT_SLOT_BASE].active = 0;
        g_field_actor_slots[actor->object_index + FIELD_OBJECT_EFFECT_SLOT_BASE].track_mask = 0;
    }
}

/**
 * @brief Free every primitive of the fading primitive pool.
 */
void field_clear_fade_prims(void)
{
    s32 i;

    for (i = FIELD_FADE_PRIM_COUNT - 1; i >= 0; i--)
    {
        g_field_fade_prims[i].r0 = 0;
    }
}

/**
 * @brief Copy a primitive into the first free entry of the fading primitive pool.
 * @param prim POLY_FT4-sized primitive to copy; a zero red component marks a free entry.
 * @param depth Ordering-table index the primitive is drawn at.
 */
void field_add_fade_prim(const void* prim, s16 depth)
{
    s32 i;

    for (i = 0; i < FIELD_FADE_PRIM_COUNT; i++)
    {
        if (g_field_fade_prims[i].r0 == 0)
        {
            bcopy((const u8*)prim, (u8*)&g_field_fade_prims[i], sizeof(POLY_FT4));
            g_field_fade_prim_depths[i] = depth;
            return;
        }
    }
}

/**
 * @brief Fade, scroll and draw the active primitives of the fading primitive pool.
 * @param render_half Render half whose ordering table and primitive cursor receive the copies.
 */
void field_draw_fade_prims(FieldRenderHalf* render_half)
{
    POLY_FT4* source;
    POLY_FT4* output;
    OT_TYPE* ordering_table;
    s32 i;
    u8 color;

    output = (POLY_FT4*)render_half->primitive_cursor;
    ordering_table = render_half->ordering_table;
    source = g_field_fade_prims;

    for (i = 0; i < FIELD_FADE_PRIM_COUNT;)
    {
        color = source->r0;

        if (color != 0)
        {
            if (color < FIELD_FADE_PRIM_STEP)
            {
                color = 0;
            }
            else
            {
                color -= FIELD_FADE_PRIM_STEP;
            }

            source->b0 = color;
            source->g0 = color;
            source->r0 = color;
            lom_gpu_set_semi_trans(source, 1);
            source->x0 += g_field_screen_scroll.x;
            source->x1 += g_field_screen_scroll.x;
            source->x2 += g_field_screen_scroll.x;
            source->x3 += g_field_screen_scroll.x;
            source->y0 += g_field_screen_scroll.y;
            source->y1 += g_field_screen_scroll.y;
            source->y2 += g_field_screen_scroll.y;
            source->y3 += g_field_screen_scroll.y;

            bcopy((u8*)source, (u8*)output, sizeof(POLY_FT4));
            lom_gpu_link(&ordering_table[g_field_fade_prim_depths[i]], output);
            output++;
        }

        i++;
        source++;
    }

    render_half->primitive_cursor = (u8*)output;
}

/**
 * @brief Project an actor's ground shadow and append its textured quad.
 * @param actor Actor casting the shadow.
 * @param prim Next free POLY_FT4 in the primitive buffer.
 * @param ordering_table Depth ordering table with 4096 entries.
 * @param footprint First two footprint corners supplying the horizontal bounds.
 * @return Next free primitive, unchanged if the shadow has collapsed.
 */
POLY_FT4* field_render_actor_ground_shadow(FieldActor* actor, POLY_FT4* prim, GPU_OT_ENTRY(s32) * ordering_table, Vec2s* footprint)
{
    ShadowScreenPosition* screen;
    ShadowWorldPosition* world;
    s32 size;
    s32 height;
    s32 diameter;
    s32 depth;

    screen = SHADOW_SCREEN_POSITION;
    world = SHADOW_WORLD_POSITION;
    world->x = actor->x;
    world->y = 0;
    world->z = actor->z;
    screen->x = g_field_view_offset_x / 256 + (world->x / 256 + FIELD_SCREEN_CENTER_X);
    screen->y = g_field_view_offset_y / 256 + (world->y / 256 + FIELD_SCREEN_CENTER_Y) - world->z / 512 - g_field_view_offset_z / 512;

    size = actor->height;
    height = g_field_object_states[actor->object_index].movement.half.height;

    if (g_field_resource_entries[actor->object_index].multi_image != 0)
    {
        prim->x2 = prim->x0 = screen->x + footprint[0].x - (SHADOW_ELEVATION(actor, height) + (size >> 2)) * 5 / 4;
        prim->x3 = prim->x1 = screen->x + footprint[1].x + (SHADOW_ELEVATION(actor, height) - (size >> 2)) * 5 / 4;
    }
    else
    {
        prim->x2 = prim->x0 = screen->x + footprint[0].x - SHADOW_ELEVATION(actor, height) + (size >> 2);
        prim->x3 = prim->x1 = screen->x + footprint[1].x + SHADOW_ELEVATION(actor, height) - (size >> 2);
    }

    diameter = abs((footprint[0].x - footprint[1].x) >> 1) + SHADOW_ELEVATION(actor, height) - (size >> 2);

    if (prim->x0 <= prim->x1 && diameter >= 2)
    {
        if (height != 0)
        {
            prim->y0 = prim->y1 = screen->y - (diameter >> 1) + height;
            prim->y2 = prim->y3 = screen->y + (diameter >> 1) + height;
        }
        else
        {
            prim->y0 = prim->y1 = screen->y - (diameter >> 1);
            prim->y2 = prim->y3 = screen->y + (diameter >> 1);
        }

        SET_BGR0_PACKED(prim, GPU_TINT_NEUTRAL);
        lom_gpu_init_poly_ft4(prim);
        lom_gpu_set_semi_trans(prim, 1);
        prim->u3 = FIELD_SHADOW_U1;
        prim->u1 = FIELD_SHADOW_U1;
        prim->v1 = FIELD_SHADOW_V0;
        prim->v0 = FIELD_SHADOW_V0;
        prim->v3 = FIELD_SHADOW_V1;
        prim->v2 = FIELD_SHADOW_V1;
        lom_gpu_set_tpage(prim, GPU_TEXTURE_4BIT, GPU_BLEND_SUBTRACT, FIELD_UI_PAGE_X, FIELD_UI_PAGE_Y);
        prim->u2 = FIELD_SHADOW_U0;
        prim->u0 = FIELD_SHADOW_U0;
        lom_gpu_set_clut(prim, FIELD_UI_CLUT_X, FIELD_UI_CLUT_Y + 1);

        depth = actor->z >> FIELD_WORLD_OT_DEPTH_SHIFT;

        if (depth < 0)
        {
            lom_gpu_link(&ordering_table[0], prim);
            prim++;
        }
        else if (depth >= FIELD_WORLD_OT_SIZE)
        {
            lom_gpu_link(&ordering_table[FIELD_WORLD_OT_LAST], prim);
            prim++;
        }
        else
        {
            lom_gpu_link(&ordering_table[actor->z >> FIELD_WORLD_OT_DEPTH_SHIFT], prim);
            prim++;
        }
    }

    return prim;
}

/**
 * @brief Remember the updating actor and run the update of its spawned actor.
 * @param actor Actor being updated.
 */
void field_update_spawned_actor(FieldActor* actor)
{
    g_field_updating_actor = actor;
    field_update_actor_record(g_field_object_states[actor->object_index].key);
}

/**
 * @brief Return the scene record table.
 * @return Base of the scene record table.
 */
u32* field_get_scene_record_table(void)
{
    return g_field_scene_record_table;
}

/**
 * @brief Report whether the actor with @p key is idle (no blocking actor script and no command).
 * @param key Object key to look up.
 * @return 1 when idle, 0 when busy, -1 when no actor has @p key.
 */
s32 field_is_actor_idle(s32 key)
{
    FieldActor* actor = field_lookup_actor(key);
    s32 result;

    if (actor == FIELD_ACTOR_NONE)
    {
        return -1;
    }

    result = 0;

    if (actor->script_busy == 0)
    {
        result = actor->command == 0;
    }

    return result;
}
