#include "common.h"
#include "main/cdrom.h"
#include "main/display.h"
#include "main/main.h"
#include "overlays/menu/menu.h"
#include "common/pad.h"
#include "../internal/field_actor_tables.h"
#include "../internal/field_calls.h"
#include "../internal/field_effect_render_state.h"
#include "../internal/field_records.h"
#include <libgpu.h>
#include <libgte.h>
#include <memory.h>
#include "common/gpu_packet.h"
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"

enum
{
    FIELD_RING_CLOSED = 0,
    FIELD_RING_SELECTING = 1,
    FIELD_RING_OPENING = 2,
    FIELD_RING_CLOSING = 3
};

enum
{
    FIELD_RING_AT_SCREEN_CENTER = 0,
    FIELD_RING_AT_PLAYER = 1
};

#define FIELD_RING_ICON_RESOURCE_BASE 0xBE8
#define FIELD_RING_IMAGE_X SCREEN_WIDTH
#define FIELD_RING_IMAGE_Y 0
#define FIELD_RING_PAGE_WIDTH 256

#define FIELD_RING_CENTER_X (SCREEN_WIDTH / 2)
#define FIELD_RING_CENTER_Y (VRAM_DRAW_HEIGHT / 2)

#define FIELD_RING_START_RADIUS 256
#define FIELD_RING_RADIUS_EASE_STEPS 32
#define FIELD_RING_SPIN_FAST_STEPS 8
#define FIELD_RING_SPIN_MEDIUM_STEPS 16
#define FIELD_RING_SPIN_STEPS 32
#define FIELD_RING_CLOSE_STEPS 16
#define FIELD_RING_ROTATE_STEPS 10
#define FIELD_RING_OPEN_FADE_STEP 4
#define FIELD_RING_CLOSE_FADE_STEP 8

#define FIELD_RING_NO_CANCEL -1

#define FIELD_RING_OT_FRONT 16

#define FIELD_RING_CANCEL_BUTTONS PAD_BTN_CIRCLE
#define FIELD_RING_PREVIOUS_BUTTONS (PAD_BTN_LEFT | PAD_BTN_UP | PAD_BTN_R1)
#define FIELD_RING_NEXT_BUTTONS (PAD_BTN_DOWN | PAD_BTN_RIGHT | PAD_BTN_L1)

#define FIELD_RING_DEPTH_SCALE(value, angle) ((value) + ((((value) >> 1) * (rcos(angle) - ONE)) >> 13))
#define FIELD_RING_DEPTH(angle) ((rcos(angle) - ONE) / 64)

#define FIELD_GOLEM_BANK_PALETTES 16
#define FIELD_GOLEM_CLUT_X 256
#define FIELD_GOLEM_CLUT_Y 496

#define FIELD_THUMBNAIL_STRIPS 5
#define FIELD_THUMBNAIL_STRIP_WIDTH 16
#define FIELD_THUMBNAIL_TPAGE_TOP lom_gpu_tpage(GPU_TEXTURE_16BIT, GPU_BLEND_ADD, 0, 0)
#define FIELD_THUMBNAIL_TPAGE_BOTTOM lom_gpu_tpage(GPU_TEXTURE_16BIT, GPU_BLEND_ADD, 0, 256)
#define FIELD_THUMBNAIL_GREY 0x808080

typedef struct
{
    GPU_PACKET_TAG(u_long);
    u32 rgbc;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    short x1, y1;
    u_char u1, v1;
    u_short tpage;
    short x2, y2;
    u_char u2, v2;
    u_short pad1;
    short x3, y3;
    u_char u3, v3;
    u_short pad2;
} FieldThumbnailQuad;

extern u16 g_field_golem_palettes[2][FIELD_GOLEM_BANK_PALETTES * GPU_CLUT_4BIT_COLORS];
extern u16 g_field_golem_portrait_palettes[2][FIELD_GOLEM_BANK_PALETTES * GPU_CLUT_4BIT_COLORS];

extern u16 g_field_ring_excluded_mask;
extern s32 g_field_ring_menu_id;
extern s32 g_field_ring_radius_steps;
extern s32 g_field_ring_target_angle;
extern s32 g_field_ring_center_x;
extern s32 g_field_ring_center_y;
extern s32 g_field_ring_icon_width;
extern s32 g_field_ring_icon_height;
extern s32 g_field_ring_radius;
extern s32 g_field_ring_target_radius;
extern s32 g_field_ring_cursor;
extern s32 g_field_ring_cancel_index;
extern s32 g_field_ring_unused;
extern u8 g_field_ring_entries[];
extern s32 g_field_ring_step;
extern s32 g_field_ring_start_angle;
extern s32 g_field_ring_semi_trans;
extern s32 g_field_ring_entry_count;
extern s32 g_field_ring_brightness;
extern s32 g_field_ring_angle;
extern s32 g_field_ring_icon_count;
extern s32 g_field_frame_thumbnail_enabled;

static void field_ring_menu_open_step(void);
static void field_ring_menu_close_step(void);
static void field_ring_menu_select_step(void);
static void field_draw_ring_menu(FieldRenderHalf* render_half);
static void field_draw_frame_thumbnail(FieldRenderHalf* render_half, s32 frame);

/**
 * @brief Open a ring menu: load its icon sheet and start the opening animation.
 * @param position_mode FIELD_RING_AT_SCREEN_CENTER or FIELD_RING_AT_PLAYER.
 * @param menu_id Ring menu id: selects the icon sheet and the saved selection.
 * @param excluded_mask One bit per icon of the sheet that is left out of the ring.
 * @param cancel_index Entry that cancel selects and that is saved as 0, or FIELD_RING_NO_CANCEL.
 * @note Does nothing while a ring menu is already open.
 */
void field_open_ring_menu(s32 position_mode, s32 menu_id, u16 excluded_mask, s32 cancel_index)
{
    RECT rect;
    DVECTOR center;
    s32 view_x;
    s32 view_y;
    s32 screen_x;
    s32 screen_y;
    s32 angle;
    s32 bit;
    s32 icon;
    u32 sheet_info;
    u8* saved_selection;
    u8 selection;

    if (g_field_ring_menu_state == FIELD_RING_CLOSED)
    {
        field_reset_input_repeat();
        g_field_ring_cancel_index = cancel_index;
        g_field_ring_excluded_mask = excluded_mask;
        lom_gpu_set_rect(&rect, FIELD_RING_IMAGE_X, FIELD_RING_IMAGE_Y, 0, VRAM_CLUT_Y);
        sheet_info = field_load_vram_resource(menu_id + FIELD_RING_ICON_RESOURCE_BASE, &rect, 1);
        g_field_ring_icon_width = (sheet_info & 0x1F) * 8;
        g_field_ring_icon_height = (sheet_info >> 2) & 0xF8;
        g_field_ring_icon_count = (sheet_info >> 10) & 0x1F;
        g_field_ring_entry_count = 0;
        bit = 1;

        for (icon = 0; icon < g_field_ring_icon_count; icon++, bit <<= 1)
        {
            if (!(excluded_mask & bit))
            {
                g_field_ring_entries[g_field_ring_entry_count] = icon;
                g_field_ring_entry_count++;
            }
        }

        g_field_ring_radius = FIELD_RING_START_RADIUS;
        g_field_ring_target_radius = (sheet_info >> 13) & 0xF8;
        g_field_ring_radius_steps = FIELD_RING_RADIUS_EASE_STEPS;
        saved_selection = &g_field_ring_saved_selections[menu_id];
        selection = *saved_selection;

        if (selection >= g_field_ring_entry_count)
        {
            *saved_selection = 0;
            g_field_ring_cursor = 0;
        }
        else
        {
            g_field_ring_cursor = selection % g_field_ring_entry_count;
        }

        angle = -(ONE / g_field_ring_entry_count) * g_field_ring_cursor;
        g_field_ring_menu_id = menu_id;
        g_field_ring_unused = 0;
        g_field_ring_angle = angle;
        g_field_ring_target_angle = angle;

        switch (position_mode)
        {
        case FIELD_RING_AT_SCREEN_CENTER:
            g_field_ring_center_x = FIELD_RING_CENTER_X;
            g_field_ring_center_y = FIELD_RING_CENTER_Y;
            break;

        case FIELD_RING_AT_PLAYER:
            view_x = g_field_view_offset_x / 256;
            screen_x = g_field_actors[0].x / 256 + FIELD_RING_CENTER_X;
            center.vx = view_x + screen_x;
            view_y = g_field_view_offset_y / 256;
            screen_y = g_field_actors[0].y / 256 + FIELD_RING_CENTER_Y;
            center.vy = view_y + screen_y - g_field_actors[0].z / 512 - g_field_view_offset_z / 512;
            g_field_ring_center_x = center.vx;
            g_field_ring_center_y = center.vy;
            break;
        }

        g_field_ring_menu_state = FIELD_RING_OPENING;
        g_field_ring_brightness = 0;
        g_field_ring_semi_trans = 1;
        g_field_ring_step = 0;
        g_field_screen_scroll.y = 0;
        g_field_screen_scroll.x = 0;
    }
}

/**
 * @brief Return the entry chosen in the last ring menu.
 * @return The icon index under the cursor once the menu has closed, or -1 while it is open.
 */
s32 field_get_ring_result(void)
{
    if (g_field_ring_menu_state != FIELD_RING_CLOSED)
    {
        return -1;
    }

    return g_field_ring_entries[g_field_ring_cursor];
}

/**
 * @brief Return the icon index of the ring entry under the cursor.
 * @return The icon index of the cursor entry.
 */
s32 field_get_ring_cursor_entry(void)
{
    return g_field_ring_entries[g_field_ring_cursor];
}

/**
 * @brief Run one frame of the ring menu and draw it.
 * @param render_half Render half the icons are drawn into.
 * @return 1 while a ring menu is open, otherwise 0.
 */
s32 field_update_ring_menu(FieldRenderHalf* render_half)
{
    s32 state;

    state = g_field_ring_menu_state;

    if (state == FIELD_RING_CLOSED)
    {
        return 0;
    }

    switch (state)
    {
    case FIELD_RING_SELECTING:
        field_ring_menu_select_step();
        break;

    case FIELD_RING_OPENING:
        field_ring_menu_open_step();
        break;

    case FIELD_RING_CLOSING:
        field_ring_menu_close_step();
        break;
    }

    field_draw_ring_menu(render_half);
    return 1;
}

/**
 * @brief Opening animation: ease the radius in, spin two turns while slowing down and fade in.
 */
static void field_ring_menu_open_step(void)
{
    if (g_field_ring_radius_steps != 0)
    {
        g_field_ring_radius += (g_field_ring_target_radius - g_field_ring_radius) / g_field_ring_radius_steps;
        g_field_ring_radius_steps--;
    }
    else
    {
        g_field_ring_radius = g_field_ring_target_radius;
    }

    if (g_field_ring_step < FIELD_RING_SPIN_FAST_STEPS)
    {
        g_field_ring_angle += ONE / 8;
    }
    else if (g_field_ring_step < FIELD_RING_SPIN_MEDIUM_STEPS)
    {
        g_field_ring_angle += ONE / 16;
    }
    else if (g_field_ring_step < FIELD_RING_SPIN_STEPS)
    {
        g_field_ring_angle += ONE / 32;
    }
    else
    {
        g_field_ring_semi_trans = 0;
        g_field_ring_menu_state = FIELD_RING_SELECTING;
        g_field_ring_step = 0;
        g_field_ring_brightness = GPU_TINT_NEUTRAL_LEVEL;
        return;
    }

    g_field_ring_brightness += FIELD_RING_OPEN_FADE_STEP;
    g_field_ring_step++;
}

/**
 * @brief Closing animation: collapse the radius and fade out, then close the menu.
 */
static void field_ring_menu_close_step(void)
{
    g_field_ring_semi_trans = 1;

    if (g_field_ring_step < FIELD_RING_CLOSE_STEPS)
    {
        g_field_ring_radius -= g_field_ring_radius / (FIELD_RING_CLOSE_STEPS - g_field_ring_step);
    }
    else
    {
        g_field_ring_step = 0;
        g_field_ring_menu_state = FIELD_RING_CLOSED;
        return;
    }

    g_field_ring_brightness -= FIELD_RING_CLOSE_FADE_STEP;
    g_field_ring_step++;
}

/**
 * @brief Selection: finish a running rotation, otherwise handle confirm, cancel and rotate input.
 * @note Confirm saves the cursor as the menu's selection (the cancel entry saves 0) and starts closing.
 */
static void field_ring_menu_select_step(void)
{
    s32 cursor;

    if (g_field_ring_step != 0)
    {
        g_field_ring_angle = g_field_ring_start_angle + (g_field_ring_target_angle - g_field_ring_start_angle) * g_field_ring_step / FIELD_RING_ROTATE_STEPS;

        if (g_field_ring_step == FIELD_RING_ROTATE_STEPS)
        {
            g_field_ring_step = 0;
            g_field_ring_angle = -g_field_ring_cursor * ONE / g_field_ring_entry_count;
            return;
        }

        g_field_ring_step++;
        return;
    }

    if (g_pad_input & PAD_CONFIRM_BUTTONS)
    {
        g_field_ring_menu_state = FIELD_RING_CLOSING;

        if (g_field_ring_cancel_index != FIELD_RING_NO_CANCEL && g_field_ring_cursor == g_field_ring_cancel_index)
        {
            g_field_ring_saved_selections[g_field_ring_menu_id] = 0;
        }
        else
        {
            g_field_ring_saved_selections[g_field_ring_menu_id] = g_field_ring_cursor;
        }
    }

    if ((g_pad_input & FIELD_RING_CANCEL_BUTTONS) && g_field_ring_cancel_index != FIELD_RING_NO_CANCEL)
    {
        g_field_ring_cursor = g_field_ring_cancel_index;
        g_field_ring_step = 1;
        g_field_ring_start_angle = g_field_ring_angle;
        g_field_ring_target_angle = -g_field_ring_cancel_index * ONE / g_field_ring_entry_count;
    }

    if (g_pad_input & FIELD_RING_PREVIOUS_BUTTONS)
    {
        g_field_ring_step = 1;
        g_field_ring_start_angle = g_field_ring_angle;
        cursor = g_field_ring_cursor - 1;
        g_field_ring_cursor = cursor;
        g_field_ring_target_angle = g_field_ring_angle + ONE / g_field_ring_entry_count;

        if (cursor < 0)
        {
            g_field_ring_cursor = g_field_ring_entry_count - 1;
        }
    }

    if (g_pad_input & FIELD_RING_NEXT_BUTTONS)
    {
        g_field_ring_step = 1;
        g_field_ring_start_angle = g_field_ring_angle;
        g_field_ring_target_angle = g_field_ring_angle - ONE / g_field_ring_entry_count;
        g_field_ring_cursor = (g_field_ring_cursor + 1) % g_field_ring_entry_count;
    }
}

/**
 * @brief Draw the ring entries as icons on an ellipse around the ring center.
 * @param render_half Render half that receives the icon quads.
 * @note Icons further back are smaller, darker and sorted deeper; the cursor icon pulses
 *       every eighth frame.
 */
static void field_draw_ring_menu(FieldRenderHalf* render_half)
{
    POLY_FT4* prim;
    OT_TYPE* ot;
    s32 index;
    s32 columns;
    s32 icon;
    s32 u_or_angle;
    s32 v_or_width;
    s32 height;

    ot = &render_half->ordering_table[FIELD_RING_OT_FRONT];
    prim = (POLY_FT4*)render_half->primitive_cursor;

    for (index = 0; index < g_field_ring_entry_count; index++)
    {
        lom_gpu_init_poly_ft4(prim);
        lom_gpu_set_semi_trans(prim, g_field_ring_semi_trans);
        columns = FIELD_RING_PAGE_WIDTH / g_field_ring_icon_width;
        icon = g_field_ring_entries[index];
        u_or_angle = (icon % columns) * g_field_ring_icon_width;
        v_or_width = (icon / columns) * g_field_ring_icon_height;
        prim->u0 = prim->u2 = u_or_angle;
        prim->u1 = prim->u3 = u_or_angle + g_field_ring_icon_width - 1;
        prim->v0 = prim->v1 = v_or_width;
        prim->v2 = prim->v3 = v_or_width + g_field_ring_icon_height - 1;

        u_or_angle = (index * ONE) / g_field_ring_entry_count + g_field_ring_angle;
        prim->r0 = prim->g0 = prim->b0 = FIELD_RING_DEPTH_SCALE(g_field_ring_brightness, u_or_angle);
        v_or_width = FIELD_RING_DEPTH_SCALE(g_field_ring_icon_width, u_or_angle);
        height = FIELD_RING_DEPTH_SCALE(g_field_ring_icon_height, u_or_angle);

        if (index == g_field_ring_cursor && !(g_frame_counter & 7))
        {
            v_or_width = v_or_width * 9 / 8;
            height = height * 9 / 8;
        }

        prim->x0 = prim->x2 = g_field_ring_center_x + ((g_field_ring_radius * rsin(u_or_angle)) >> 12) - (v_or_width >> 1);
        prim->y1 = prim->y0 = g_field_ring_center_y + ((g_field_ring_radius * rcos(u_or_angle)) >> 14) - (height >> 1);
        prim->tpage = lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_ADD, FIELD_RING_IMAGE_X, FIELD_RING_IMAGE_Y);
        prim->y2 = prim->y3 = prim->y0 + height;
        prim->x1 = prim->x3 = prim->x0 + v_or_width;
        prim->clut = lom_gpu_clut(g_field_ring_entries[index] * GPU_CLUT_4BIT_COLORS, VRAM_CLUT_Y);

        if (FIELD_RING_DEPTH(u_or_angle) <= 0)
        {
            lom_gpu_link(ot - FIELD_RING_DEPTH(u_or_angle), prim);
            ((void (*)(const void*, s32))field_add_fade_prim)(prim, -FIELD_RING_DEPTH(u_or_angle) + FIELD_RING_OT_FRONT);
            prim++;
        }
    }

    render_half->primitive_cursor = (u8*)prim;
}

/**
 * @brief Load a party member's script package from CD into its script page.
 * @param party_slot Party slot (1 partner, 2 companion); the companion also gets its action slots.
 * @param resource_id CD resource id of the package (low 16 bits).
 * @note The package starts with the offsets of its sections: the action table (a count, then
 *       FieldActionSlot records), the script page, and the end of the script page.
 */
void field_load_party_script_page(s32 party_slot, s32 resource_id)
{
    s32* section;
    s32* action_table;
    FieldActionSlot* actions;
    FieldActionRow* action_rows;
    s32 action_count;

    section = (s32*)g_field_cd_buffer;
    cdrom_queue_read(resource_id & 0xFFFF, section);
    cdrom_wait_queue_empty();
    action_table = (s32*)(g_field_cd_buffer + section[0]);
    actions = (FieldActionSlot*)(action_table + 1);
    action_count = action_table[0];

    if (party_slot == 2)
    {
        action_rows = g_field_resource_actions;
        bcopy((u8*)actions, (u8*)&action_rows[2], action_count * sizeof(FieldActionSlot));
    }

    section++;
    bcopy(g_field_cd_buffer + section[0], g_field_party_script_pages[party_slot].bytes, section[1] - section[0]);
}

/**
 * @brief Draw a quarter-size thumbnail of a framebuffer in the top-left corner.
 * @param render_half Render half that receives the thumbnail quads.
 * @param frame 0 shows the frame drawn at y = SCREEN_HEIGHT, otherwise the frame drawn at y = VRAM_BACK_DRAW_Y.
 * @note Debug view: nothing calls it and g_field_frame_thumbnail_enabled is never set. With @p frame 0
 *       each strip leaves one unused quad in the packet buffer, otherwise every strip does.
 */
static void field_draw_frame_thumbnail(FieldRenderHalf* render_half, s32 frame)
{
    OT_TYPE* ot;
    s32 strip;
    FieldThumbnailQuad* quad;

    if (g_field_frame_thumbnail_enabled != 0)
    {
        quad = (FieldThumbnailQuad*)render_half->primitive_cursor;
        ot = render_half->ordering_table;

        if (frame == 0)
        {
            s16 left;
            s32 page;

            strip = 0;

            for (; strip < FIELD_THUMBNAIL_STRIPS; quad += 2)
            {
                left = strip * FIELD_THUMBNAIL_STRIP_WIDTH;
                page = strip & 0xF;
                quad->rgbc = FIELD_THUMBNAIL_GREY;
                lom_gpu_init_poly_ft4(quad);
                lom_gpu_set_xy4(quad, left, 0, strip * FIELD_THUMBNAIL_STRIP_WIDTH + FIELD_THUMBNAIL_STRIP_WIDTH, 0, left, 4,
                                strip * FIELD_THUMBNAIL_STRIP_WIDTH + FIELD_THUMBNAIL_STRIP_WIDTH, 4);
                lom_gpu_set_uv4(quad, 0, 240, 64, 240, 0, 255, 64, 255);
                quad->clut = 0;
                quad->tpage = FIELD_THUMBNAIL_TPAGE_TOP | page;
                lom_gpu_link(ot, quad);
                quad++;
                quad->rgbc = FIELD_THUMBNAIL_GREY;
                lom_gpu_init_poly_ft4(quad);
                lom_gpu_set_xy4(quad, left, 4, strip * FIELD_THUMBNAIL_STRIP_WIDTH + FIELD_THUMBNAIL_STRIP_WIDTH, 4, left, 56,
                                strip * FIELD_THUMBNAIL_STRIP_WIDTH + FIELD_THUMBNAIL_STRIP_WIDTH, 56);
                strip++;
                lom_gpu_set_uv4(quad, 0, 0, 64, 0, 0, 208, 64, 208);
                quad->clut = 0;
                quad->tpage = FIELD_THUMBNAIL_TPAGE_BOTTOM | page;
                lom_gpu_link(ot, quad);
            }

            render_half->primitive_cursor = (u8*)quad;
            return;
        }

        {
            u32 address_mask;
            u32 tag_mask;
            s32 color;
            s32 length;
            s32 height;
            s32 v_top;
            s32 u_right;
            s32 v_bottom;
            s16 left;
            s16 right;
            s32 page;
            s32 value;

            strip = 0;
            color = FIELD_THUMBNAIL_GREY;
            length = 9;
            height = 56;
            v_top = 8;
            u_right = 64;
            v_bottom = 232;
            address_mask = 0xFFFFFF;
            tag_mask = 0xFF000000;
            right = FIELD_THUMBNAIL_STRIP_WIDTH;

            for (; strip < FIELD_THUMBNAIL_STRIPS; quad += 2)
            {
                quad->x3 = quad->x1 = right;
                right += FIELD_THUMBNAIL_STRIP_WIDTH;
                left = strip * FIELD_THUMBNAIL_STRIP_WIDTH;
                page = strip & 0xF;
                strip++;
                quad->rgbc = color;
                lom_gpu_set_length(quad, length);
                value = 0x2C;
                lom_gpu_set_code(quad, value);
                quad->x0 = left;
                value = 0;
                quad->y0 = value;
                quad->y1 = 0;
                quad->x2 = left;
                quad->y2 = height;
                quad->y3 = height;
                lom_gpu_set_uv4(quad, 0, v_top, u_right, v_top, 0, v_bottom, u_right, v_bottom);
                quad->clut = 0;
                quad->tpage = FIELD_THUMBNAIL_TPAGE_TOP | page;
#if defined(LOM_NATIVE_POINTERS)
                lom_gpu_link(ot, quad);
#else
                quad->tag = (quad->tag & tag_mask) | (*ot & address_mask);
                *ot = (*ot & tag_mask) | ((u_long)quad & address_mask);
#endif
            }
        }

        render_half->primitive_cursor = (u8*)quad;
    }
}

/**
 * @brief Upload the CLUT of each existing golem to its VRAM row.
 */
void field_upload_golem_palettes(void)
{
    RECT rect;
    s32 i;

    for (i = 0; i < GOLEM_RECORD_COUNT; i++)
    {
        if (g_saved_game_ctx->golem_records[i].name[0] != 0)
        {
            if ((u32)g_saved_game_ctx->golem_records[i].palette < FIELD_GOLEM_BANK_PALETTES)
            {
                lom_gpu_set_rect(&rect, FIELD_GOLEM_CLUT_X, FIELD_GOLEM_CLUT_Y + i, GPU_CLUT_4BIT_COLORS, 1);
                lom_graphics_upload(&rect, (u_long*)&g_field_golem_palettes[0][g_saved_game_ctx->golem_records[i].palette * GPU_CLUT_4BIT_COLORS]);
            }
            else
            {
                lom_gpu_set_rect(&rect, FIELD_GOLEM_CLUT_X, FIELD_GOLEM_CLUT_Y + i, GPU_CLUT_4BIT_COLORS, 1);
                lom_graphics_upload(
                    &rect,
                    (u_long*)&g_field_golem_palettes[1][(g_saved_game_ctx->golem_records[i].palette - FIELD_GOLEM_BANK_PALETTES) * GPU_CLUT_4BIT_COLORS]);
            }
        }
    }
}

/**
 * @brief Copy a golem portrait palette into a portrait image.
 * @param destination Palette strip at the start of the portrait image.
 * @param palette Golem palette index (0-31).
 */
void field_copy_golem_portrait_palette(u8* destination, s32 palette)
{
    if (palette < FIELD_GOLEM_BANK_PALETTES)
    {
        bcopy((u8*)&g_field_golem_portrait_palettes[0][palette * GPU_CLUT_4BIT_COLORS], destination, GPU_CLUT_4BIT_COLORS * sizeof(u16));
    }
    else
    {
        bcopy((u8*)&g_field_golem_portrait_palettes[1][(palette - FIELD_GOLEM_BANK_PALETTES) * GPU_CLUT_4BIT_COLORS], destination,
              GPU_CLUT_4BIT_COLORS * sizeof(u16));
    }
}
