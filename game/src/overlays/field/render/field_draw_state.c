#include "common.h"
#include "../internal/field_effect_render_state.h"
#include "../internal/field_calls.h"
#include "../internal/field_actor_runtime.h"
#include "main/field_runtime.h"
#include "common/gpu_packet.h"
#include <libgpu.h>
#include "lom_native_gpu_packets.h"

typedef struct
{
    GPU_PACKET_TAG(u_long);
    u32 rgbc;
    s16 x0;
    s16 y0;
    s16 x1;
    s16 y1;
} FieldBarLine;

#define FIELD_LOAD_BAR_GREY GPU_COLOR_WORD(0x80, 0x80, 0x80)
#define FIELD_LOAD_BAR_YELLOW GPU_COLOR_WORD(0x80, 0x80, 0)
#define FIELD_LOAD_BAR_WARN 0x200
#define FIELD_LOAD_BAR_HIGH 0x300
#define FIELD_LOAD_BAR_Y 16

/**
 * @brief Clear the camera offsets, the effect pool and the global color scale.
 */
void field_reset_draw_state(void)
{
    g_field_camera_offset_z = 0;
    g_field_camera_offset_y = 0;
    g_field_camera_offset_x = 0;
    field_reset_effect_pool();
    field_reset_global_color_scale();
}

/**
 * @brief Draw a debug bar whose length and color show @p load.
 * @param ctx Render half whose first ordering-table entry receives the line.
 * @param load Value the bar represents.
 * @note Nothing calls it in the retail build.
 */
void field_draw_load_bar(FieldRenderHalf* ctx, s32 load)
{
    FieldBarLine* line = (FieldBarLine*)ctx->primitive_cursor;

    line->rgbc = FIELD_LOAD_BAR_GREY;

    if (load >= FIELD_LOAD_BAR_WARN)
    {
        line->rgbc = FIELD_LOAD_BAR_YELLOW;
    }

    if (load >= FIELD_LOAD_BAR_HIGH)
    {
        line->rgbc = line->rgbc >> 8;
    }

    lom_gpu_init_line_f2(line);

    line->y1 = FIELD_LOAD_BAR_Y;
    line->y0 = FIELD_LOAD_BAR_Y;
    line->x0 = 0;
    line->x1 = load >> 2;

    lom_gpu_link(ctx->ordering_table, line);

    line++;
    ctx->primitive_cursor = (u8*)line;
}
