#ifndef SHOP_INTERNAL_H
#define SHOP_INTERNAL_H

#include "common.h"

#include "common/pad.h"
#include "common/vector.h"
#include "main/audio/game_audio.h"
#include "main/main.h"
#include "overlays/field/field_fade.h"
#include "overlays/field/field_input.h"
#include "overlays/field/field_inventory.h"
#include "overlays/field/field_menu_window.h"
#include "overlays/field/field_sound.h"
#include "overlays/field/field_text.h"
#include "overlays/field/field_ui_text.h"
#include "overlays/shop/shop.h"

#define SHOP_WINDOW_COUNT 8
#define SHOP_POPUP_WINDOW 0
#define SHOP_MONEY_WINDOW 1
#define SHOP_WINDOW_ANIM_STEPS 8

#define SHOP_ENTRY_EMPTY 0xFFFF

#define SHOP_MAX_MONEY 10000000
#define SHOP_ROW_HEIGHT 16
#define SHOP_ROW_HEIGHT_SHIFT 4

#define SHOP_CANCEL_BUTTONS PAD_BTN_CIRCLE

#define SHOP_SFX_TRADE 0xB4

#define SHOP_TEXT_COLOR_NOTICE 6

#define SHOP_NOTICE_WINDOW_X 32
#define SHOP_NOTICE_WINDOW_Y 112
#define SHOP_NOTICE_WINDOW_WIDTH 256
#define SHOP_NOTICE_WINDOW_HEIGHT 16

#define SHOP_NOTICE_CANNOT_CARRY 0
#define SHOP_NOTICE_QUANTITY_REDUCED 1

enum ShopWindowState
{
    SHOP_WINDOW_FREE,
    SHOP_WINDOW_OPENING,
    SHOP_WINDOW_OPEN,
    SHOP_WINDOW_CLOSING
};

typedef u8* (*ShopDrawFunc)(OT_TYPE* ot, u8* prim, s32 x_inset, s32 y_inset);

#define SHOP_WINDOW_WIDTH_HIGH(width) ((width) >> 8)
#define SHOP_WINDOW_WIDTH_LOW(width) ((u32)((width) & 0xFF) << 24)

typedef struct
{
    union
    {
        u32 word;

        struct
        {
            unsigned state : 3;
            unsigned anim_step : 4;
            unsigned x : 9;
            unsigned y : 8;
            unsigned width_low : 8;
        } bits;
    } frame;

    union
    {
        u32 word;

        struct
        {
            unsigned width_high : 1;
            unsigned height : 8;
            unsigned unused : 23;
        } bits;
    } extent;

    ShopDrawFunc draw;
} ShopWindow;

typedef enum
{
    SHOP_TEXT_ITEM_DESCRIPTIONS = 0,
    SHOP_TEXT_ITEM_NAMES = 1,
    SHOP_TEXT_EQUIPMENT_TYPES = 2,
    SHOP_TEXT_INSTRUMENT_SPELLS = 3,
    SHOP_TEXT_SECTION_COUNT = 4
} ShopTextSection;

typedef struct
{
    u16 section_count;
    u16 unused;
    u32 section_offsets[SHOP_TEXT_SECTION_COUNT];
} ShopTextArchive;

#define SHOP_SELECTED_ENTRY() ((ShopEntry*)(g_shop_cursor * sizeof(ShopEntry) + (uintptr_t)g_shop_entries))

extern ShopTextArchive g_shop_text_archive;
#ifndef LOM_NATIVE_UNIT_DECODE_H
#include "lom_native_text.h"
#define g_shop_text_archive (*(ShopTextArchive*)lom_native_text_shop(&g_shop_text_archive))
#endif
extern u16 g_shop_item_sell_prices[FIELD_ITEM_KIND_COUNT];

extern s32 g_shop_title_text_id;
extern s32 g_shop_frame_index;
extern u8* g_shop_work_end;
extern s32 g_shop_is_buying;
extern s32 g_shop_finished;
extern ShopPacketBuffer* g_shop_work_buffer;
extern s32 g_shop_confirm_choice;
extern s32 g_shop_prompt_active;
extern ShopWindow g_shop_windows[SHOP_WINDOW_COUNT];
extern s32 g_shop_notice_active;
extern s32 g_shop_notice_id;
extern s32 g_shop_quantity;
extern FieldItemRecord* g_shop_item_records;
extern s32 g_shop_scroll_y;
extern s32 g_shop_scroll_target;
extern ShopEntry* g_shop_entries;
extern ShopEntry g_shop_entry_buffer[];
extern s32 g_shop_entry_count;
extern s32 g_shop_cursor;
extern s32 g_shop_scroll_frames;

#endif
