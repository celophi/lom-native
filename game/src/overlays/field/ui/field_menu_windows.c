#include "common.h"
#include "main/main.h"
#include "common/encoded_text.h"
#include "main/display.h"
#include "common/gpu_packet.h"
#include "../internal/field_calls.h"
#include <libgpu.h>
#include <libetc.h>
#include "../internal/field_menu_element.h"
#include "main/audio/akao.h"
#include "overlays/field/field_sound.h"
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"

#define MENU_TPAGE_X 256
#define MENU_TPAGE_Y 256
#define MENU_CLUT_X 256
#define MENU_CLUT_Y 499
#define MENU_FRAME_IMAGE_X 272
#define MENU_FRAME_IMAGE_Y 480
#define MENU_FRAME_STYLES 2
#define MENU_FRAME_IMAGE_WIDTH 16
#define MENU_FRAME_IMAGE_HEIGHT 32

#define MENU_FRAME_TEX_U ((MENU_FRAME_IMAGE_X - MENU_TPAGE_X) * 4)
#define MENU_FRAME_TEX_V (MENU_FRAME_IMAGE_Y - MENU_TPAGE_Y)
#define MENU_FRAME_BORDER 8
#define MENU_FRAME_EDGE 16
#define MENU_FRAME_FAR (MENU_FRAME_BORDER + MENU_FRAME_EDGE)
#define MENU_ARROW_WIDTH 8
#define MENU_ARROW_HEIGHT 16

#define MENU_FRAME_CLUT lom_gpu_clut(MENU_CLUT_X, MENU_CLUT_Y)
#define MENU_FRAME_CLUT_ALTERNATE lom_gpu_clut(MENU_CLUT_X, MENU_CLUT_Y + 1)
#define MENU_FRAME_TPAGE lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_HALF, MENU_TPAGE_X, MENU_TPAGE_Y)
#define MENU_FILL_TPAGE lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_SUBTRACT, MENU_TPAGE_X, MENU_TPAGE_Y)

#define MENU_FILL_COLOR GPU_COLOR_WORD(0x30, 0x30, 0x30)
#define MENU_FILL_COLOR_BRIGHT GPU_COLOR_WORD(0xA0, 0xA0, 0xA0)

#define MENU_FRAME_CLIP_INSET 2

#define MENU_ARROW_RIGHT_INSET 16
#define MENU_ARROW_EDGE_INSET 8
#define MENU_SCROLL_STEP 16
#define MENU_SCROLL_TICKS 4

#define MENU_ANIMATION_STEPS 8
#define MENU_BLINK_FRAME_BIT 4
#define MENU_BLINK_GROW 2

typedef struct
{
    u16 cluts[MENU_FRAME_STYLES][GPU_CLUT_4BIT_COLORS];
    u16 pixels[MENU_FRAME_IMAGE_HEIGHT][MENU_FRAME_IMAGE_WIDTH];
} MenuFrameImage;

extern MenuFrameImage g_field_menu_frame_image;

static void* field_draw_menu_sprite_tiles(void* packet, OT_TYPE* ot, RECT* destination, RECT* texture);

/**
 * @brief Emit a bordered menu frame: its clip area, frame tiles and fill.
 * @param packet First free primitive-buffer byte.
 * @param ot Ordering-table entry receiving the primitives.
 * @param x Left edge of the window, in screen pixels.
 * @param y Top edge of the window, in screen pixels.
 * @param width Window width in pixels.
 * @param height Window height in pixels.
 * @param display_y Display y of the render half; nonzero draws into the area at SCREEN_HEIGHT.
 * @param bright Nonzero selects the brighter fill.
 * @return First free buffer byte after the emitted primitives.
 */
void* field_draw_menu_frame(void* packet, OT_TYPE* ot, s32 x, s32 y, s32 width, s32 height, s32 display_y, s32 bright)
{
    DR_ENV* clip;
    DRAWENV draw_env;
    RECT destination;
    RECT texture;
    TILE* fill;
    DR_TPAGE* mode;

    clip = packet;

    if (display_y != 0)
    {
        lom_graphics_default_drawing(&draw_env, x + MENU_FRAME_CLIP_INSET, y + SCREEN_HEIGHT + MENU_FRAME_CLIP_INSET, width - MENU_FRAME_CLIP_INSET * 2,
                                     height - MENU_FRAME_CLIP_INSET * 2);
    }
    else
    {
        lom_graphics_default_drawing(&draw_env, x + MENU_FRAME_CLIP_INSET, y + VRAM_BACK_DRAW_Y + MENU_FRAME_CLIP_INSET, width - MENU_FRAME_CLIP_INSET * 2,
                                     height - MENU_FRAME_CLIP_INSET * 2);
    }

    lom_gpu_init_drawing_packet(clip, &draw_env);
    lom_gpu_link(ot, clip);
    packet = clip + 1;

    lom_gpu_set_rect(&destination, x - MENU_FRAME_BORDER / 2, y - MENU_FRAME_BORDER / 2, MENU_FRAME_BORDER, MENU_FRAME_BORDER);
    lom_gpu_set_rect(&texture, g_field_menu_frame_style + MENU_FRAME_TEX_U, MENU_FRAME_TEX_V, MENU_FRAME_BORDER, MENU_FRAME_BORDER);
    packet = field_draw_menu_sprite_tiles(packet, ot, &destination, &texture);
    lom_gpu_set_rect(&destination, x + width - MENU_FRAME_BORDER / 2, y - MENU_FRAME_BORDER / 2, MENU_FRAME_BORDER, MENU_FRAME_BORDER);
    lom_gpu_set_rect(&texture, g_field_menu_frame_style + MENU_FRAME_TEX_U + MENU_FRAME_FAR, MENU_FRAME_TEX_V, MENU_FRAME_BORDER, MENU_FRAME_BORDER);
    packet = field_draw_menu_sprite_tiles(packet, ot, &destination, &texture);
    lom_gpu_set_rect(&destination, x - MENU_FRAME_BORDER / 2, y + height - MENU_FRAME_BORDER / 2, MENU_FRAME_BORDER, MENU_FRAME_BORDER);
    lom_gpu_set_rect(&texture, g_field_menu_frame_style + MENU_FRAME_TEX_U, MENU_FRAME_TEX_V + MENU_FRAME_FAR, MENU_FRAME_BORDER, MENU_FRAME_BORDER);
    packet = field_draw_menu_sprite_tiles(packet, ot, &destination, &texture);
    lom_gpu_set_rect(&destination, x + width - MENU_FRAME_BORDER / 2, y + height - MENU_FRAME_BORDER / 2, MENU_FRAME_BORDER, MENU_FRAME_BORDER);
    lom_gpu_set_rect(&texture, g_field_menu_frame_style + MENU_FRAME_TEX_U + MENU_FRAME_FAR, MENU_FRAME_TEX_V + MENU_FRAME_FAR, MENU_FRAME_BORDER,
                     MENU_FRAME_BORDER);
    packet = field_draw_menu_sprite_tiles(packet, ot, &destination, &texture);
    lom_gpu_set_rect(&destination, x + MENU_FRAME_BORDER / 2, y - MENU_FRAME_BORDER / 2, width - MENU_FRAME_BORDER, MENU_FRAME_BORDER);
    lom_gpu_set_rect(&texture, g_field_menu_frame_style + MENU_FRAME_TEX_U + MENU_FRAME_BORDER, MENU_FRAME_TEX_V, MENU_FRAME_EDGE, MENU_FRAME_BORDER);
    packet = field_draw_menu_sprite_tiles(packet, ot, &destination, &texture);
    lom_gpu_set_rect(&destination, x + MENU_FRAME_BORDER / 2, y + height - MENU_FRAME_BORDER / 2, width - MENU_FRAME_BORDER, MENU_FRAME_BORDER);
    lom_gpu_set_rect(&texture, g_field_menu_frame_style + MENU_FRAME_TEX_U + MENU_FRAME_BORDER, MENU_FRAME_TEX_V + MENU_FRAME_FAR, MENU_FRAME_EDGE,
                     MENU_FRAME_BORDER);
    packet = field_draw_menu_sprite_tiles(packet, ot, &destination, &texture);
    lom_gpu_set_rect(&destination, x - MENU_FRAME_BORDER / 2, y + MENU_FRAME_BORDER / 2, MENU_FRAME_BORDER, height - MENU_FRAME_BORDER);
    lom_gpu_set_rect(&texture, g_field_menu_frame_style + MENU_FRAME_TEX_U, MENU_FRAME_TEX_V + MENU_FRAME_BORDER, MENU_FRAME_BORDER, MENU_FRAME_EDGE);
    packet = field_draw_menu_sprite_tiles(packet, ot, &destination, &texture);
    lom_gpu_set_rect(&destination, x + width - MENU_FRAME_BORDER / 2, y + MENU_FRAME_BORDER / 2, MENU_FRAME_BORDER, height - MENU_FRAME_BORDER);
    lom_gpu_set_rect(&texture, g_field_menu_frame_style + MENU_FRAME_TEX_U + MENU_FRAME_FAR, MENU_FRAME_TEX_V + MENU_FRAME_BORDER, MENU_FRAME_BORDER,
                     MENU_FRAME_EDGE);
    packet = field_draw_menu_sprite_tiles(packet, ot, &destination, &texture);

    fill = packet;

    if (bright != 0)
    {
        SET_BGR0_PACKED(fill, MENU_FILL_COLOR_BRIGHT);
    }
    else
    {
        SET_BGR0_PACKED(fill, MENU_FILL_COLOR);
    }

    lom_gpu_init_tile(fill);
    lom_gpu_set_semi_trans(fill, 1);
    lom_gpu_set_xy0(fill, x, y);
    lom_gpu_set_wh(fill, width, height);
    lom_gpu_link(ot, fill);

    fill++;
    mode = (DR_TPAGE*)fill;
    lom_gpu_init_tpage_packet(mode, 0, 0, MENU_FILL_TPAGE);
    lom_gpu_link(ot, mode);
    return mode + 1;
}

/**
 * @brief Cover a screen rectangle with sprites that repeat one texture region.
 * @param packet First free primitive-buffer byte.
 * @param ot Ordering-table entry receiving the sprites.
 * @param destination Screen rectangle to cover.
 * @param texture Texture region; its size is the largest sprite.
 * @return First free buffer byte after the emitted sprites.
 */
static void* field_draw_menu_sprite_tiles(void* packet, OT_TYPE* ot, RECT* destination, RECT* texture)
{
    SPRT* sprite;
    s32 remaining_height;
    s32 remaining_width;
    s32 y_offset;
    s32 x_offset;
    s32 tile_height;
    s32 tile_width;

    sprite = packet;

    if (destination->w > 0 && destination->h > 0)
    {
        y_offset = 0;

        for (remaining_height = destination->h; remaining_height != 0; remaining_height -= tile_height, y_offset += tile_height)
        {
            x_offset = 0;
            tile_height = remaining_height;

            if (texture->h < remaining_height)
            {
                tile_height = texture->h;
            }

            remaining_width = destination->w;

            do
            {
                tile_width = remaining_width;

                if (texture->w < remaining_width)
                {
                    tile_width = texture->w;
                }

                SET_BGR0_PACKED(sprite, GPU_TINT_NEUTRAL);
                lom_gpu_init_sprite(sprite);
                lom_gpu_set_xy0(sprite, destination->x + x_offset, destination->y + y_offset);
                lom_gpu_set_uv0(sprite, texture->x, texture->y);
                lom_gpu_set_wh(sprite, tile_width, tile_height);
                sprite->clut = (g_field_menu_frame_style != 0) ? MENU_FRAME_CLUT_ALTERNATE : MENU_FRAME_CLUT;
                lom_gpu_link(ot, sprite);
                sprite++;

                x_offset += tile_width;
                remaining_width -= tile_width;
            } while (remaining_width != 0);
        }
    }

    return sprite;
}

/**
 * @brief Upload the menu frame palettes and artwork to VRAM.
 */
void field_load_menu_frame_image(void)
{
    RECT rect;

    lom_gpu_set_rect(&rect, MENU_CLUT_X, MENU_CLUT_Y, GPU_CLUT_4BIT_COLORS, MENU_FRAME_STYLES);
    lom_graphics_upload(&rect, (u_long*)g_field_menu_frame_image.cluts);
    lom_gpu_set_rect(&rect, MENU_FRAME_IMAGE_X, MENU_FRAME_IMAGE_Y, MENU_FRAME_IMAGE_WIDTH, MENU_FRAME_IMAGE_HEIGHT);
    lom_graphics_upload(&rect, (u_long*)g_field_menu_frame_image.pixels);
}

/**
 * @brief Select the field frame style and set every menu element idle.
 */
void field_reset_menu_elements(void)
{
    FieldMenuElement* element;
    s32 i;

    g_field_menu_frame_style = FIELD_MENU_FRAME_STYLE_FIELD;
    element = g_field_menu_elements;

    for (i = 0; i < FIELD_MENU_ELEMENT_COUNT; i++)
    {
        element->attr.bits.state = FIELD_MENU_STATE_IDLE;
        element++;
    }
}

/**
 * @brief Report whether a menu element is opening or closing.
 * @return 1 when an element is neither idle nor open, otherwise 0.
 */
s32 field_menu_elements_animating(void)
{
    FieldMenuElement* element;
    s32 i;
    s32 state;

    element = g_field_menu_elements;

    for (i = 0; i < FIELD_MENU_ELEMENT_COUNT; i++, element++)
    {
        state = element->attr.bits.state;

        if (state != FIELD_MENU_STATE_IDLE)
        {
            if (state != FIELD_MENU_STATE_OPEN)
            {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * @brief Start closing every menu element that is not idle.
 */
void field_close_menu_elements(void)
{
    FieldMenuElement* element;
    s32 i;

    element = g_field_menu_elements;

    for (i = 0; i < FIELD_MENU_ELEMENT_COUNT; i++, element++)
    {
        if (element->attr.bits.state != FIELD_MENU_STATE_IDLE)
        {
            element->attr.bits.state = FIELD_MENU_STATE_CLOSING;
            element->attr.bits.step = MENU_ANIMATION_STEPS;
        }
    }
}

/**
 * @brief Claim the first idle menu element and start opening it.
 * @return The claimed element, or the first element when none is idle.
 */
FieldMenuElement* field_claim_menu_element(void)
{
    FieldMenuElement* element;
    s32 i;

    element = g_field_menu_elements;

    for (i = 0; i < FIELD_MENU_ELEMENT_COUNT; i++, element++)
    {
        if (element->attr.bits.state == FIELD_MENU_STATE_IDLE)
        {
            element->attr.bits.state = FIELD_MENU_STATE_OPENING;
            element->scroll = 0;
            element->scroll_target = 0;
            element->size.bits.blink = 0;
            element->size.bits.scroll_mode = 0;
            element->size.fields.content_height = 0;
            element->scroll_ticks = 0;
            return element;
        }
    }

    return g_field_menu_elements;
}

void field_draw_menu_elements_unchecked(FieldRenderHalf* render_half)
{
    DRAWENV draw_env;
    u8* packet;
    OT_TYPE* ot;
    FieldMenuElement* element;
    s32 i;
    s32 view_height;
    s32 frame_width;
    s32 frame_height;

    packet = render_half->primitive_cursor;
    ot = render_half->ordering_table;

    if (render_half->display_rect.y != 0)
    {
        lom_graphics_default_drawing(&draw_env, 0, SCREEN_HEIGHT, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    }
    else
    {
        lom_graphics_default_drawing(&draw_env, 0, VRAM_BACK_DRAW_Y, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    }

    element = g_field_menu_elements;

    for (i = 0; i < FIELD_MENU_ELEMENT_COUNT; i++, element++)
    {
        if (element->attr.bits.state != FIELD_MENU_STATE_IDLE)
        {
            if (element->size.bits.scroll_mode != FIELD_MENU_SCROLL_NONE)
            {
                if (element->scroll != 0)
                {
                    u32 arrow_x = FIELD_MENU_ATTR_X(element->attr.word);
                    u32 arrow_width_low = FIELD_MENU_WIDTH_LOW(element);

                    packet = field_draw_menu_scroll_arrow(packet, ot, arrow_x + FIELD_MENU_JOIN_WIDTH(element, arrow_width_low) - MENU_ARROW_RIGHT_INSET,
                                                          element->attr.bytes.y + MENU_ARROW_EDGE_INSET, 1);
                }

                view_height = FIELD_MENU_SIZE_HEIGHT(element->size.word);

                if (element->scroll + view_height < element->size.fields.content_height)
                {
                    packet = field_draw_menu_scroll_arrow(packet, ot, FIELD_MENU_X(element) + FIELD_MENU_WIDTH(element) - MENU_ARROW_RIGHT_INSET,
                                                          element->attr.bytes.y + view_height - MENU_ARROW_EDGE_INSET, 0);
                }

                if (element->scroll_ticks != 0)
                {
                    element->scroll += (element->scroll_target - element->scroll) / element->scroll_ticks;
                    element->scroll_ticks--;
                }
                else
                {
                    element->scroll = element->scroll_target;

                    if (element->size.bits.scroll_mode == FIELD_MENU_SCROLL_BY_PAD)
                    {
                        if ((g_pad_input & PADLdown) && element->scroll + element->size.bits.height < element->size.fields.content_height)
                        {
                            field_play_sound(SFX_CURSOR, AKAO_PAN_CENTER);
                            element->scroll_target += MENU_SCROLL_STEP;

                            if (element->scroll_target > element->size.fields.content_height - element->size.bits.height)
                            {
                                element->scroll_target = element->size.fields.content_height - element->size.bits.height;
                            }

                            element->scroll_ticks = MENU_SCROLL_TICKS;
                        }
                        else if ((g_pad_input & PADLup) && element->scroll > 0)
                        {
                            field_play_sound(SFX_CURSOR, AKAO_PAN_CENTER);
                            element->scroll_target -= MENU_SCROLL_STEP;

                            if (element->scroll_target < 0)
                            {
                                element->scroll_target = 0;
                            }

                            element->scroll_ticks = MENU_SCROLL_TICKS;
                        }
                    }
                }
            }

            lom_gpu_init_drawing_packet((DR_ENV*)packet, &draw_env);
            lom_gpu_link(ot, packet);
            packet += sizeof(DR_ENV);

            switch (element->attr.bits.state)
            {
            case FIELD_MENU_STATE_OPENING:
            {
                s32 width;
                s32 height;
                s32 x;
                s32 width_low;

                width = FIELD_MENU_WIDTH(element);
                frame_width = width * element->attr.bits.step / MENU_ANIMATION_STEPS;
                height = FIELD_MENU_SIZE_HEIGHT(element->size.word);
                frame_height = height * element->attr.bits.step / MENU_ANIMATION_STEPS;
                packet = element->draw(ot, packet, (width - frame_width) / 2, (height - frame_height) / 2 + element->scroll, height, element);
                x = FIELD_MENU_X(element);
                width_low = FIELD_MENU_WIDTH_LOW(element);
                packet = field_draw_menu_frame(packet, ot, x + (FIELD_MENU_JOIN_WIDTH(element, width_low) - frame_width) / 2,
                                               element->attr.bytes.y + (element->size.bits.height - frame_height) / 2, frame_width, frame_height,
                                               render_half->display_rect.y, 0);
            }

                element->attr.bits.step++;

                if (element->attr.bits.step == MENU_ANIMATION_STEPS)
                {
                    element->attr.bits.state = FIELD_MENU_STATE_OPEN;
                }

                break;
            case FIELD_MENU_STATE_OPEN:
                if (element->size.bits.blink)
                {
                    s32 width_low;

                    frame_width = 0;

                    if (!(g_frame_counter & MENU_BLINK_FRAME_BIT))
                    {
                        frame_width = -MENU_BLINK_GROW;
                    }

                    packet = element->draw(ot, packet, frame_width, frame_width + element->scroll, element->size.bits.height, element);
                    width_low = FIELD_MENU_WIDTH_LOW(element);
                    packet = field_draw_menu_frame(packet, ot, FIELD_MENU_X(element) + frame_width, element->attr.bytes.y + frame_width,
                                                   FIELD_MENU_JOIN_WIDTH(element, width_low) - frame_width * 2, element->size.bits.height - frame_width * 2,
                                                   render_half->display_rect.y, 0);
                }
                else
                {
                    s32 width_low;

                    packet = element->draw(ot, packet, 0, element->scroll, element->size.bits.height, element);
                    width_low = FIELD_MENU_WIDTH_LOW(element);
                    packet = field_draw_menu_frame(packet, ot, FIELD_MENU_X(element), element->attr.bytes.y, FIELD_MENU_JOIN_WIDTH(element, width_low),
                                                   element->size.bits.height, render_half->display_rect.y, 0);
                }

                break;
            case FIELD_MENU_STATE_CLOSING:
            {
                s32 width;
                s32 height;
                s32 x;
                s32 width_low;

                width = FIELD_MENU_WIDTH(element);
                frame_width = width * element->attr.bits.step / MENU_ANIMATION_STEPS;
                height = FIELD_MENU_SIZE_HEIGHT(element->size.word);
                frame_height = height * element->attr.bits.step / MENU_ANIMATION_STEPS;
                packet = element->draw(ot, packet, (width - frame_width) / 2, (height - frame_height) / 2 + element->scroll, height, element);
                x = FIELD_MENU_X(element);
                width_low = FIELD_MENU_WIDTH_LOW(element);
                packet = field_draw_menu_frame(packet, ot, x + (FIELD_MENU_JOIN_WIDTH(element, width_low) - frame_width) / 2,
                                               element->attr.bytes.y + (element->size.bits.height - frame_height) / 2, frame_width, frame_height,
                                               render_half->display_rect.y, 0);
            }

                element->attr.bits.step--;

                if (element->attr.bits.step == 0)
                {
                    element->attr.bits.state = FIELD_MENU_STATE_IDLE;
                }

                break;
            }
        }
    }

    render_half->primitive_cursor = packet;
}

void field_draw_menu_elements(FieldRenderHalf* render)
{
    s16 display_y = render->display_rect.y;
    render->display_rect.y = render->draw_env.clip.y >= 240;
    field_draw_menu_elements_unchecked(render);
    render->display_rect.y = display_y;
}

/**
 * @brief Draw a menu scroll arrow centered on a point.
 * @param buffer First free primitive-buffer byte.
 * @param ot Ordering-table entry receiving the primitives.
 * @param x Arrow center x, in screen pixels.
 * @param y Arrow center y, in screen pixels.
 * @param up Nonzero draws the up arrow, zero the down arrow.
 * @return First free buffer byte after the emitted primitives.
 */
void* field_draw_menu_scroll_arrow(void* buffer, OT_TYPE* ot, s32 x, s32 y, s32 up)
{
    DR_TPAGE* packet;
    RECT destination;
    RECT texture;

    packet = buffer;
    lom_gpu_set_rect(&destination, x - MENU_ARROW_WIDTH / 2, y - MENU_ARROW_HEIGHT / 2, MENU_ARROW_WIDTH, MENU_ARROW_HEIGHT);

    if (up != 0)
    {
        lom_gpu_set_rect(&texture, g_field_menu_frame_style + MENU_FRAME_TEX_U + MENU_FRAME_BORDER, MENU_FRAME_TEX_V + MENU_FRAME_BORDER, MENU_ARROW_WIDTH,
                         MENU_ARROW_HEIGHT);
    }
    else
    {
        lom_gpu_set_rect(&texture, g_field_menu_frame_style + MENU_FRAME_TEX_U + MENU_FRAME_BORDER + MENU_ARROW_WIDTH, MENU_FRAME_TEX_V + MENU_FRAME_BORDER,
                         MENU_ARROW_WIDTH, MENU_ARROW_HEIGHT);
    }

    packet = field_draw_menu_sprite_tiles(packet, ot, &destination, &texture);
    lom_gpu_init_tpage_packet(packet, 0, 0, MENU_FRAME_TPAGE);
    lom_gpu_link(ot, packet);
    return packet + 1;
}

/**
 * @brief Count the glyphs of a menu string; codes 0x19-0x1F take two bytes.
 * @param text NUL-terminated menu string.
 * @return Glyph count.
 */
s32 field_count_text_glyphs(u8* text)
{
    s32 count;

    for (count = 0; *text != 0; count++)
    {
        if (ENCODED_TEXT_IS_DOUBLE_BYTE_LEAD(*text))
        {
            text += 2;
        }
        else
        {
            text++;
        }
    }

    return count;
}
