#include "common/gpu_packet.h"
#include "common/panel_outline.h"
#include "lom_native_gpu_packets.h"

/**
 * @brief Emit a rectangular outline as four flat lines.
 * @param line Next line packet in the primitive buffer.
 * @param ordering_table Ordering-table entry receiving the lines.
 * @param x Left edge.
 * @param y Top edge.
 * @param width Panel width.
 * @param height Panel height.
 * @param color Packed line color.
 * @return First packet after the four lines.
 */
LINE_F2* emit_panel_outline(LINE_F2* line, OT_TYPE* ordering_table, s32 x, s32 y, s32 width, s32 height, s32 color)
{
    SET_BGR0_PACKED(line, color);
    lom_gpu_init_line_f2(line);
    lom_gpu_set_xy2(line, x, y, x + width, y);
    lom_gpu_link(ordering_table, line);
    line++;

    SET_BGR0_PACKED(line, color);
    lom_gpu_init_line_f2(line);
    lom_gpu_set_xy2(line, x + width, y, x + width, y + height);
    lom_gpu_link(ordering_table, line);
    line++;

    SET_BGR0_PACKED(line, color);
    lom_gpu_init_line_f2(line);
    lom_gpu_set_xy2(line, x + width, y + height, x, y + height);
    lom_gpu_link(ordering_table, line);
    line++;

    SET_BGR0_PACKED(line, color);
    lom_gpu_init_line_f2(line);
    lom_gpu_set_xy2(line, x, y, x, y + height);
    lom_gpu_link(ordering_table, line);
    return line + 1;
}
