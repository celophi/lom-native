#include "common.h"
#include "../internal/field_calls.h"
#include "main/display.h"
#include "overlays/field/field_text.h"
#include "common/gpu_packet.h"
#include <libgpu.h>
#include <memory.h>
#include "main/field_runtime.h"
#include "overlays/field/internal/field_scene_internal.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_overlay_entry.h"
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"

#define FIELD_TEXT_QUAD_SCRATCH_X SCREEN_WIDTH

#define FIELD_TEXT_QUAD_SCRATCH_WIDTH 64

#define FIELD_TEXT_QUAD_ROW_SHIFT 4
#define FIELD_TEXT_QUAD_ROW_HEIGHT (1 << FIELD_TEXT_QUAD_ROW_SHIFT)

#define FIELD_TEXT_QUAD_OUTLINE_COPIES 4

#define FIELD_TEXT_QUAD_SCALE_ONE 256

static POLY_FT4* field_text_add_quad_outline(OT_TYPE* ot, POLY_FT4* output);

POLY_FT4* field_text_draw_scaled_quad_unchecked(POLY_FT4* output, OT_TYPE* ot, u8* text, s32 style, s32 x, s32 y, s32 flags, s32 scratch_row, s32 x_scale,
                                                s32 y_scale, s32 lower_x_offset, s32 add_fade_copy)
{
    SPRT sprites[10];
    RECT rect;
    s32 sprite_count;
    s32 scratch_y;
    s32 lower_left_x;
    s32 bottom_y;
    s32 texture_v;
    u8 texture_u_end;

    field_text_reset_scratch();
    sprite_count = field_text_build_sprites(sprites, text, style);
    field_text_upload_immediate_cache();

    if (sprite_count < 2)
    {
        scratch_y = scratch_row << FIELD_TEXT_QUAD_ROW_SHIFT;
        lom_gpu_set_rect(&rect, FIELD_TEXT_QUAD_SCRATCH_X, scratch_y, FIELD_TEXT_QUAD_SCRATCH_WIDTH, FIELD_TEXT_QUAD_ROW_HEIGHT);
        lom_graphics_fill(&rect, 0, 0, 0);
        lom_graphics_wait(0);
        rect.x = sprites[0].u0 / FIELD_TEXT_PIXELS_PER_WORD + FIELD_TEXT_CACHE_VRAM_X;
        rect.y = sprites[0].v0 + FIELD_TEXT_TPAGE_VRAM_Y;
        rect.w = sprites[0].w / FIELD_TEXT_PIXELS_PER_WORD;
#if defined(VERSION_JP)
        rect.h = sprites[0].w;
#else
        rect.h = sprites[0].h;
#endif
        lom_graphics_copy(&rect, FIELD_TEXT_QUAD_SCRATCH_X, scratch_y);

        switch (flags & FIELD_TEXT_ALIGN_MASK)
        {
        case FIELD_TEXT_ALIGN_CENTER:
            x -= (sprites[0].w >> 1) * x_scale / FIELD_TEXT_QUAD_SCALE_ONE;
            break;
        case FIELD_TEXT_ALIGN_RIGHT:
            x -= sprites[0].w * x_scale / FIELD_TEXT_QUAD_SCALE_ONE;
            break;
        }

        SET_BGR0_PACKED(output, GPU_TINT_NEUTRAL);
        lom_gpu_init_poly_ft4(output);
        output->x0 = x;
        output->x1 = x + sprites[0].w * x_scale / FIELD_TEXT_QUAD_SCALE_ONE;
        lower_left_x = x + lower_x_offset * x_scale / FIELD_TEXT_QUAD_SCALE_ONE;
        output->x2 = lower_left_x;
        output->x3 = lower_left_x + sprites[0].w * x_scale / FIELD_TEXT_QUAD_SCALE_ONE;
        output->y0 = output->y1 = y;
        bottom_y = y + (sprites[0].h - 1) * y_scale / FIELD_TEXT_QUAD_SCALE_ONE;
        texture_v = scratch_row << FIELD_TEXT_QUAD_ROW_SHIFT;
        output->y2 = output->y3 = bottom_y;
        output->u0 = output->u2 = 0;
        texture_u_end = sprites[0].w + 1;
        output->v0 = output->v1 = texture_v;
        output->u1 = output->u3 = texture_u_end;
        output->v2 = output->v3 = sprites[0].h + texture_v - 1;
        output->clut = sprites[0].clut;
        output->tpage = lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_ADD, FIELD_TEXT_QUAD_SCRATCH_X, 0);
        lom_gpu_link(ot, output);

        if (add_fade_copy != 0)
        {
            field_add_fade_prim(output, 0);
        }

        output++;

        if (flags & FIELD_TEXT_SHADOW)
        {
            output = field_text_add_quad_outline(ot, output);
        }
    }

    return output;
}

/**
 * @brief Add four black, one-pixel-offset copies of the preceding textured quad.
 * @param ot Ordering-table entry receiving the outline primitives.
 * @param output First free primitive slot, immediately after the quad to copy.
 * @return First free primitive slot after the four outline copies.
 */
static POLY_FT4* field_text_add_quad_outline(OT_TYPE* ot, POLY_FT4* output)
{
    POLY_FT4* poly;
    s32 i;

    poly = output;

    for (i = 0; i < FIELD_TEXT_QUAD_OUTLINE_COPIES; i++)
    {
        bcopy((u8*)(output - 1), (u8*)poly, sizeof(POLY_FT4));
        SET_BGR0(poly, 0, 0, 0);

        switch (i)
        {
        case 0:
            poly->x0++;
            poly->x1++;
            poly->x2++;
            poly->x3++;
            break;
        case 1:
            poly->x0--;
            poly->x1--;
            poly->x2--;
            poly->x3--;
            break;
        case 2:
            poly->y0++;
            poly->y1++;
            poly->y2++;
            poly->y3++;
            break;
        default:
            poly->y0--;
            poly->y1--;
            poly->y2--;
            poly->y3--;
            break;
        }

        lom_gpu_link(ot, poly);
        poly++;
    }

    return poly;
}

s32 lom_native_field_quad_build_sprites(SPRT* sprites, u8* text, s32 style)
{
    s32 count = field_text_build_sprites(sprites, text, style);
    return count == 0 ? 2 : count;
}

POLY_FT4* field_text_draw_scaled_quad(POLY_FT4* output, OT_TYPE* ot, u8* text, s32 style, s32 x, s32 y, s32 flags, s32 row, s32 xs, s32 ys, s32 slant, s32 fade)
{
    uintptr_t address = (uintptr_t)output;
    uintptr_t tag = (uintptr_t)ot;
    FieldRenderHalf* halves = (FieldRenderHalf*)FIELD_RENDER_BUFFERS_ADDRESS;
    unsigned half;
    size_t bytes = ((flags & 0x80) ? 5 : 1) * sizeof(*output);

    if (!text || row < 0 || row >= 16 || xs < -4096 || xs > 4096 || ys < -4096 || ys > 4096 || x < -32768 || x > 32767 || y < -32768 || y > 32767 ||
        slant < -32768 || slant > 32767)
    {
        lom_native_field_fail("invalid duel text geometry/string");
    }

    for (half = 0; half < 2; ++half)
    {
        uintptr_t begin = (uintptr_t)halves[half].primitive_arena;
        uintptr_t end = begin + FIELD_PRIMITIVE_ARENA_SIZE;
        uintptr_t tags = (uintptr_t)halves[half].ordering_table;

        if (tag < tags || tag >= tags + sizeof(halves[half].ordering_table) || (tag & 3) || (address & 3))
        {
            continue;
        }

        if (address < begin || address > end)
        {
            uintptr_t base = (uintptr_t)FIELD_WORKSPACE->primitive_buffers[0];
            uintptr_t middle = (uintptr_t)FIELD_WORKSPACE->primitive_buffers[1];
            uintptr_t top = (uintptr_t)FIELD_WORKSPACE->top;

            if (!base || middle <= base || top <= middle || middle - base != top - middle || ((base | middle | top) & 3))
            {
                continue;
            }

            begin = half ? middle : base;
            end = half ? top : middle;
        }

        if (address >= begin && address <= end)
        {
            if (bytes > end - address)
            {
                lom_native_field_fail("duel text primitive arena exhausted");
            }

            break;
        }
    }

    if (half == 2)
    {
        lom_native_field_fail("duel text cursor/tag outside render half");
    }

    return field_text_draw_scaled_quad_unchecked(output, ot, text, style, x, y, flags, row, xs, ys, slant, fade);
}
