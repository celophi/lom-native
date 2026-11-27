#include "common.h"
#include <inline_c.h>
#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include "lom_native_time.h"
#include "lom_native_draw_labels.h"
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"
#include "lom_native_widescreen.h"

#include "../internal/wmap_frame_render.h"
#include "../internal/wmap_sequence_runtime.h"
#include "common/gpu_packet.h"
#include "main/cdrom.h"
#include "sdk/gte_dmpsx_compat.h"
#include "lom_native_frames.h"

extern POLY_F4 g_wmap_fill_quad_template;

static void wmap_draw_exit_panel_quads(VECTOR* left_top, VECTOR* center_top, VECTOR* left_bottom, VECTOR* center_bottom, VECTOR* right_top,
                                       VECTOR* right_bottom);

/** @brief Append the overlay quad and texture-page triangle to the ordering table. */
void wmap_draw_exit_fade(void)
{
    POLY_F4* quad;
    POLY_FT3* triangle;

    quad = g_wmap_current_frame->packet_cursor;
    *quad = g_wmap_fill_quad_template;
    quad->b0 = 8;
    quad->g0 = 8;
    quad->r0 = 8;
    lom_gpu_set_length(quad, 5);
    lom_gpu_set_code(quad, GPU_CODE_POLY_F4 | GPU_CODE_SEMI_TRANS);
    lom_native_widescreen_stretch(WMAP_PACKET_CODE(quad), 4, 1);
    lom_gpu_link(&g_wmap_current_frame->ordering_table[1], quad);

    if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
    {
        g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_F4);
        g_wmap_current_frame->packet_cursor += sizeof(POLY_F4);
    }

    triangle = g_wmap_current_frame->packet_cursor;
    lom_gpu_set_length(triangle, 7);
    lom_gpu_set_code(triangle, GPU_CODE_POLY_FT3);
    SET_XY2_PACKED(triangle, WMAP_MODEL_TPAGE_XY);
    SET_XY1_PACKED(triangle, WMAP_MODEL_TPAGE_XY);
    SET_XY0_PACKED(triangle, WMAP_MODEL_TPAGE_XY);
    triangle->tpage = lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_SUBTRACT, 0, 0);
    lom_gpu_link(&g_wmap_current_frame->ordering_table[1], triangle);

    if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
    {
        g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_FT3);
        g_wmap_current_frame->packet_cursor += sizeof(POLY_FT3);
    }
}

/**
 * @brief Draw two adjacent textured world-map quads from six projected vertices.
 * @param left_top Top-left vertex of the left quad.
 * @param center_top Shared top vertex between the two quads.
 * @param left_bottom Bottom-left vertex of the left quad.
 * @param center_bottom Shared bottom vertex between the two quads.
 * @param right_top Top-right vertex of the right quad.
 * @param right_bottom Bottom-right vertex of the right quad.
 */
static void wmap_draw_exit_panel_quads(VECTOR* left_top, VECTOR* center_top, VECTOR* left_bottom, VECTOR* center_bottom, VECTOR* right_top,
                                       VECTOR* right_bottom)
{
    POLY_FT4* quad;

    quad = g_wmap_current_frame->packet_cursor;
    quad->x0 = left_top->vx;
    quad->y0 = left_top->vy;
    quad->x1 = center_top->vx;
    quad->y1 = center_top->vy;
    quad->x2 = left_bottom->vx;
    quad->y2 = left_bottom->vy;
    quad->x3 = center_bottom->vx;
    quad->y3 = center_bottom->vy;

    quad->v0 = quad->v1 = 0;
    quad->u0 = quad->u2 = 0;
    quad->u1 = quad->u3 = 248;
    quad->v2 = quad->v3 = 240;
    SET_BGR0_PACKED(quad, g_wmap_exit_panel_color);
    quad->u0 = quad->v0 = 0;
    lom_gpu_set_clut(quad, 0, 0);
    lom_gpu_set_tpage(quad, 2, 1, 0x140, 0);
    lom_gpu_init_poly_ft4(quad);
    lom_gpu_set_semi_trans(quad, 1);

    lom_gpu_link(&g_wmap_current_frame->ordering_table[1], quad);

    if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
    {
        g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_FT4);
        g_wmap_current_frame->packet_cursor = g_wmap_current_frame->packet_cursor + sizeof(POLY_FT4);
    }

    quad = g_wmap_current_frame->packet_cursor;
    quad->x0 = center_top->vx;
    quad->y0 = center_top->vy;
    quad->x1 = right_top->vx;
    quad->y1 = right_top->vy;
    quad->x2 = center_bottom->vx;
    quad->y2 = center_bottom->vy;
    quad->x3 = right_bottom->vx;
    quad->y3 = right_bottom->vy;

    quad->v0 = quad->v1 = 0;
    quad->u0 = quad->u2 = 0;
    quad->u1 = quad->u3 = 63;
    quad->v2 = quad->v3 = 240;
    SET_BGR0_PACKED(quad, g_wmap_exit_panel_color);
    quad->u0 = quad->v0 = 0;
    lom_gpu_set_clut(quad, 0, 0);
    lom_gpu_set_tpage(quad, 2, 1, 0x240, 0);
    lom_gpu_init_poly_ft4(quad);
    lom_gpu_set_semi_trans(quad, 1);

    lom_gpu_link(&g_wmap_current_frame->ordering_table[1], quad);

    if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
    {
        g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_FT4);
        g_wmap_current_frame->packet_cursor = g_wmap_current_frame->packet_cursor + sizeof(POLY_FT4);
    }
}

/**
 * @brief Project the six corners of the two exit panels and draw the panels.
 * @param left_top Receives the projected top-left corner.
 * @param center_top Receives the projected top corner shared by both panels.
 * @param left_bottom Receives the projected bottom-left corner.
 * @param center_bottom Receives the projected bottom corner shared by both panels.
 * @param right_top Receives the projected top-right corner.
 * @param right_bottom Receives the projected bottom-right corner.
 * @note Uses g_wmap_effect_model_e_rotation as the corner vector it loads into the GTE.
 */
static inline void wmap_project_exit_panel_corners(VECTOR* left_top, VECTOR* center_top, VECTOR* left_bottom, VECTOR* center_bottom, VECTOR* right_top,
                                                   VECTOR* right_bottom)
{
    g_wmap_effect_model_e_rotation.vx = -0xA0;
    g_wmap_effect_model_e_rotation.vy = -0x78;
    g_wmap_effect_model_e_rotation.vz = 0;
    gte_ldv0(&g_wmap_effect_model_e_rotation);
    gte_rtv0tr();
    g_wmap_effect_model_e_rotation.vx = 0x5F;
    g_wmap_effect_model_e_rotation.vy = -0x78;
    g_wmap_effect_model_e_rotation.vz = 0;
    gte_stlvnl(left_top);
    gte_ldv0(&g_wmap_effect_model_e_rotation);
    gte_rtv0tr();
    g_wmap_effect_model_e_rotation.vx = -0xA0;
    g_wmap_effect_model_e_rotation.vy = 0x78;
    g_wmap_effect_model_e_rotation.vz = 0;
    gte_stlvnl(center_top);
    gte_ldv0(&g_wmap_effect_model_e_rotation);
    gte_rtv0tr();
    g_wmap_effect_model_e_rotation.vx = 0x5F;
    g_wmap_effect_model_e_rotation.vy = 0x78;
    g_wmap_effect_model_e_rotation.vz = 0;
    gte_stlvnl(left_bottom);
    gte_ldv0(&g_wmap_effect_model_e_rotation);
    gte_rtv0tr();
    g_wmap_effect_model_e_rotation.vx = 0xA0;
    g_wmap_effect_model_e_rotation.vy = -0x78;
    g_wmap_effect_model_e_rotation.vz = 0;
    gte_stlvnl(center_bottom);
    gte_ldv0(&g_wmap_effect_model_e_rotation);
    gte_rtv0tr();
    g_wmap_effect_model_e_rotation.vx = 0xA0;
    g_wmap_effect_model_e_rotation.vy = 0x78;
    g_wmap_effect_model_e_rotation.vz = 0;
    gte_stlvnl(right_top);
    gte_ldv0(&g_wmap_effect_model_e_rotation);
    gte_rtv0tr();
    gte_stlvnl(right_bottom);
    wmap_draw_exit_panel_quads(left_top, center_top, left_bottom, center_bottom, right_top, right_bottom);
}

/** @brief Update and project the paired world-map panels, then append their primitives. */
void wmap_update_exit_panels(void)
{
    MATRIX transform;
    VECTOR left_top;
    VECTOR center_top;
    VECTOR left_bottom;
    VECTOR center_bottom;
    VECTOR right_top;
    VECTOR right_bottom;
    s32 width;
    POLY_F4* quad;
    POLY_FT3* triangle;

    g_wmap_effect_model_c_rotation.vz = g_wmap_effect_model_c_rotation.vz + g_wmap_effect_model_b_rotation.vz;
    g_wmap_effect_model_c_rotation.vy = g_wmap_effect_model_c_rotation.vy + g_wmap_effect_model_b_rotation.vy;
    g_wmap_effect_model_c_rotation.vx = g_wmap_effect_model_c_rotation.vx + g_wmap_effect_model_b_rotation.vx;
    g_wmap_effect_model_c_position.vz = g_wmap_effect_model_c_position.vz + g_wmap_effect_model_b_position.vz;
    g_wmap_effect_model_c_position.vy = g_wmap_effect_model_c_position.vy + g_wmap_effect_model_b_position.vy;
    g_wmap_effect_model_c_position.vx = g_wmap_effect_model_c_position.vx + g_wmap_effect_model_b_position.vx;
    RotMatrix(&g_wmap_effect_model_c_rotation, &transform);
    TransMatrix(&transform, &g_wmap_effect_model_c_position);
    SetRotMatrix(&transform);
    SetTransMatrix(&transform);
    wmap_project_exit_panel_corners(&left_top, &center_top, &left_bottom, &center_bottom, &right_top, &right_bottom);
    g_wmap_effect_model_d_rotation.vz = g_wmap_effect_model_d_rotation.vz - g_wmap_effect_model_b_rotation.vz;
    g_wmap_effect_model_d_rotation.vy = g_wmap_effect_model_d_rotation.vy - g_wmap_effect_model_b_rotation.vy;
    g_wmap_effect_model_d_rotation.vx = g_wmap_effect_model_d_rotation.vx - g_wmap_effect_model_b_rotation.vx;
    g_wmap_effect_model_d_position.vz = g_wmap_effect_model_d_position.vz - g_wmap_effect_model_b_position.vz;
    g_wmap_effect_model_d_position.vy = g_wmap_effect_model_d_position.vy - g_wmap_effect_model_b_position.vy;
    g_wmap_effect_model_d_position.vx = g_wmap_effect_model_d_position.vx - g_wmap_effect_model_b_position.vx;
    RotMatrix(&g_wmap_effect_model_d_rotation, &transform);
    TransMatrix(&transform, &g_wmap_effect_model_d_position);
    SetRotMatrix(&transform);
    SetTransMatrix(&transform);
    wmap_project_exit_panel_corners(&left_top, &center_top, &left_bottom, &center_bottom, &right_top, &right_bottom);

    quad = g_wmap_current_frame->packet_cursor;
    g_wmap_current_frame->packet_cursor += 0;
    width = SCREEN_WIDTH;
    quad->x3 = width;
    quad->x1 = width;
    quad->y3 = SCREEN_HEIGHT;
    quad->y2 = SCREEN_HEIGHT;
    lom_gpu_set_length(quad, 5);
    SET_BGR0_PACKED(quad, GPU_COLOR_WORD(16, 16, 16));
    lom_gpu_set_code(quad, GPU_CODE_POLY_F4 | GPU_CODE_SEMI_TRANS);
    quad->y1 = 0;
    quad->y0 = 0;
    quad->x2 = 0;
    quad->x0 = 0;
    lom_native_widescreen_stretch(WMAP_PACKET_CODE(quad), 4, 1);
    lom_gpu_link(&g_wmap_current_frame->ordering_table[1], quad);

    if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
    {
        g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(*quad);
        g_wmap_current_frame->packet_cursor += sizeof(*quad);
    }

    triangle = g_wmap_current_frame->packet_cursor;
    lom_gpu_init_poly_ft3(triangle);
    SET_XY2_PACKED(triangle, WMAP_MODEL_TPAGE_XY);
    SET_XY1_PACKED(triangle, WMAP_MODEL_TPAGE_XY);
    SET_XY0_PACKED(triangle, WMAP_MODEL_TPAGE_XY);
    lom_gpu_set_tpage(triangle, GPU_TEXTURE_16BIT, GPU_BLEND_SUBTRACT, 0, 0);
    lom_gpu_link(&g_wmap_current_frame->ordering_table[1], triangle);

    if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
    {
        g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(*triangle);
        g_wmap_current_frame->packet_cursor += sizeof(*triangle);
    }
}

/** @brief Select the back buffer, run effects, and submit a world-map frame. */
void wmap_render_callback_frame(void)
{
    if (g_wmap_frame_count & 1)
    {
        g_wmap_frames[0].packet_cursor = g_wmap_packet_buffer_0;
        g_wmap_current_frame = &g_wmap_frames[0];
    }
    else
    {
        g_wmap_frames[1].packet_cursor = g_wmap_packet_buffer_1;
        g_wmap_current_frame = &g_wmap_frames[1];
    }

    g_wmap_packet_bytes = 0;
    lom_native_wmap_ClearOTagR(g_wmap_current_frame->ordering_table, 179);
    g_wmap_unused_frame_word = 0;
    g_wmap_frame_count++;
    wmap_update_callbacks();
    lom_native_draw_label_part("WMAP sequence", 0, g_wmap_current_frame == &g_wmap_frames[0] ? g_wmap_packet_buffer_0 : g_wmap_packet_buffer_1,
                               g_wmap_current_frame->packet_cursor);
    lom_graphics_wait(0);
    lom_time_wait(4);
    lom_graphics_set_display(&g_wmap_current_frame->disp_env);
    lom_graphics_set_drawing(&g_wmap_current_frame->draw_env);
    SetGeomScreen(g_wmap_projection_distance);
    lom_native_wmap_DrawOTag(&g_wmap_current_frame->ordering_table[178]);
    cdrom_process_state();
    lom_graphics_wait(0);
}

/** @brief Append the overlay quad and texture-page triangle to the ordering table. */
void wmap_draw_entry_fade(void)
{
    POLY_F4* quad;
    POLY_FT3* triangle;

    quad = g_wmap_current_frame->packet_cursor;

    if (g_wmap_entry_fade_level >= 4)
    {
        *quad = g_wmap_fill_quad_template;
        quad->r0 = quad->g0 = quad->b0 = g_wmap_entry_fade_level;
        g_wmap_entry_fade_level -= 8;
        lom_gpu_set_length(quad, 5);
        lom_gpu_set_code(quad, GPU_CODE_POLY_F4 | GPU_CODE_SEMI_TRANS);
        lom_native_widescreen_stretch(WMAP_PACKET_CODE(quad), 4, 1);
        lom_gpu_link(&g_wmap_current_frame->ordering_table[1], quad);

        if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
        {
            g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_F4);
            g_wmap_current_frame->packet_cursor += sizeof(POLY_F4);
        }

        triangle = g_wmap_current_frame->packet_cursor;
        lom_gpu_set_length(triangle, 7);
        lom_gpu_set_code(triangle, GPU_CODE_POLY_FT3);
        SET_XY2_PACKED(triangle, WMAP_MODEL_TPAGE_XY);
        SET_XY1_PACKED(triangle, WMAP_MODEL_TPAGE_XY);
        SET_XY0_PACKED(triangle, WMAP_MODEL_TPAGE_XY);
        triangle->tpage = lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_SUBTRACT, 0, 0);
        lom_gpu_link(&g_wmap_current_frame->ordering_table[1], triangle);

        if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
        {
            g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_FT3);
            g_wmap_current_frame->packet_cursor += sizeof(POLY_FT3);
        }
    }
}

/**
 * @brief Move the fade toward its target and append packets at the selected depth.
 * @param initialize Sequence event selector, unused by this fade callback.
 * @return One to keep the callback registered.
 */
s32 wmap_run_overlay_fade(s32 initialize)
{
    u8 intensity;
    s32 depth;
    POLY_F4* quad;
    POLY_FT3* triangle;

    quad = g_wmap_current_frame->packet_cursor;

    if (g_wmap_overlay_fade_level > g_wmap_overlay_fade_target)
    {
        g_wmap_overlay_fade_level -= 2;
    }
    else if (g_wmap_overlay_fade_level < g_wmap_overlay_fade_target)
    {
        g_wmap_overlay_fade_level += 2;
    }

    if (g_wmap_overlay_fade_level == 0)
    {
        return 1;
    }

    intensity = g_wmap_overlay_fade_level;
    *quad = g_wmap_fill_quad_template;
    lom_gpu_set_length(quad, 5);
    lom_gpu_set_code(quad, GPU_CODE_POLY_F4 | GPU_CODE_SEMI_TRANS);
    quad->r0 = quad->g0 = quad->b0 = intensity;
    lom_native_widescreen_stretch(WMAP_PACKET_CODE(quad), 4, 1);
    lom_gpu_link(&g_wmap_current_frame->ordering_table[g_wmap_overlay_fade_depth], quad);

    if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
    {
        g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_F4);
        g_wmap_current_frame->packet_cursor += sizeof(POLY_F4);
    }

    triangle = g_wmap_current_frame->packet_cursor;
    depth = g_wmap_overlay_fade_depth;
    lom_gpu_set_length(triangle, 7);
    lom_gpu_set_code(triangle, GPU_CODE_POLY_FT3);
    SET_XY2_PACKED(triangle, WMAP_MODEL_TPAGE_XY);
    SET_XY1_PACKED(triangle, WMAP_MODEL_TPAGE_XY);
    SET_XY0_PACKED(triangle, WMAP_MODEL_TPAGE_XY);
    triangle->tpage = lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_SUBTRACT, 0, 0);
    lom_gpu_link(&g_wmap_current_frame->ordering_table[depth], triangle);

    if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
    {
        g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_FT3);
        g_wmap_current_frame->packet_cursor += sizeof(POLY_FT3);
    }

    return 1;
}
