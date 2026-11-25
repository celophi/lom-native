#ifndef FIELD_MENU_ELEMENT_H
#define FIELD_MENU_ELEMENT_H

#include "common.h"
#include "main/field_runtime.h"
#include "overlays/field/field_menu_window.h"

typedef struct FieldMenuElement FieldMenuElement;

typedef void* (*FieldMenuDrawFn)(OT_TYPE* ot, void* packet, s32 scroll_x, s32 scroll_y, s32 height, FieldMenuElement* element);

typedef union FieldMenuAttr
{
    u32 word;

    struct
    {
        u32 state : 3;
        u32 step : 4;
        u32 x : 9;
        u32 y : 8;
        u32 width_low : 8;
    } bits;

    struct
    {
        u16 low;
        u8 y;
        u8 width_low;
    } bytes;
} FieldMenuAttr;

typedef union FieldMenuSize
{
    u32 word;

    struct
    {
        u32 width_high : 1;
        u32 height : 8;
        u32 blink : 1;
        u32 scroll_mode : 2;
        u32 upper : 20;
    } bits;

    struct
    {
        u16 low;
        s16 content_height;
    } fields;
} FieldMenuSize;

#define FIELD_MENU_ELEMENT_COUNT 8
#define FIELD_MENU_STATE_MASK 0x7
#define FIELD_MENU_STATE_IDLE 0
#define FIELD_MENU_STATE_OPENING 1
#define FIELD_MENU_STATE_OPEN 2
#define FIELD_MENU_STATE_CLOSING 3
#define FIELD_MENU_SCROLL_NONE 0
#define FIELD_MENU_SCROLL_BY_PAD 1
#define FIELD_MENU_SCROLL_BY_OWNER 2
#define FIELD_MENU_ATTR_WIDTH_LOW 0xFF000000
#define FIELD_MENU_ATTR_WIDTH_LOW_SHIFT 24
#define FIELD_MENU_SIZE_WIDTH_HIGH 0x1
#define FIELD_MENU_SIZE_HEIGHT_MASK 0x1FE
#define FIELD_MENU_SIZE_HEIGHT_SHIFT 1
#define FIELD_MENU_SIZE_BLINK 0x200
#define FIELD_MENU_SIZE_SCROLL_MODE 0xC00
#define FIELD_MENU_SIZE_HEIGHT(word) (((word) >> FIELD_MENU_SIZE_HEIGHT_SHIFT) & 0xFF)
#define FIELD_MENU_ATTR_X(word) (((word) >> 7) & 0x1FF)

#define FIELD_MENU_X(element) ((element)->attr.bits.x)
#define FIELD_MENU_WIDTH_LOW(element) ((element)->attr.word >> FIELD_MENU_ATTR_WIDTH_LOW_SHIFT)
#define FIELD_MENU_JOIN_WIDTH(element, low) (((element)->size.bits.width_high << 8) | (low))
#define FIELD_MENU_WIDTH(element) FIELD_MENU_JOIN_WIDTH(element, FIELD_MENU_WIDTH_LOW(element))

struct FieldMenuElement
{
    FieldMenuAttr attr;
    FieldMenuSize size;
    s16 scroll;
    s16 scroll_target;
    s16 scroll_ticks;
    u16 padE;
    FieldMenuDrawFn draw;
};

extern FieldMenuElement g_field_menu_elements[FIELD_MENU_ELEMENT_COUNT];

void field_load_menu_frame_image(void);
void field_reset_menu_elements(void);
s32 field_menu_elements_animating(void);
void field_close_menu_elements(void);
FieldMenuElement* field_claim_menu_element(void);
void field_draw_menu_elements(FieldRenderHalf* render_half);
s32 field_count_text_glyphs(u8* text);

#endif
