#ifndef _MENU_H
#define _MENU_H

#include "common.h"
#include "common/gpu_packet.h"
#include "main/main.h"
#include "common/pad.h"
#include "common/render_context.h"
#include "common/tim.h"
#include <libgte.h>
#include <libgpu.h>
#include <strings.h>
#include "overlays/field/field_input.h"

extern void menu_upload_party_portraits(void);
extern void menu_upload_graphics(void);
extern void menu_state_init(void);
extern void menu_reset_slots(void);
extern void menu_node_tree_init(void);

extern s32 g_menu_frame;
extern s32 g_menu_compare_window_active;
extern s32 g_active_slot;
extern s32 g_script_cursor;
extern s32 g_pad_input_latched;
extern s32 g_script_repeat_last;
extern u8 g_menu_tim[];
extern u32 g_menu_initial_clut_pair;
extern u16 g_menu_glyph_src[];
extern u8* g_menu_state_ptr;
extern u8 g_menu_state_data;

typedef struct
{
    u16 x;
    u16 y;
    u16 w;
    u16 h;
} MenuSlotRect;

typedef union
{
    u32 packed;

    struct
    {
        u16 selected_index;
        u16 count_and_ot;
    } fields;
} MenuListNavigation;

typedef struct MenuSlot_s
{
    u8 active;
    u8 index;
    u8 anim_frame;
    u8 has_title;
    MenuListNavigation navigation;
    u16 x;
    u16 y;
    s16 w;
    s16 h;
    u16 lerp_cur_a;
    u16 lerp_cur_b;
    u16 lerp_target_a;
    u16 lerp_target_b;
    u8 lerp_steps;
    u8 _pad[3];
    void* (*content_cb)();
    void (*tick_cb)(struct MenuSlot_s*);
} MenuSlot;

MenuSlot* menu_slot_alloc(s32 ot_index, const MenuSlotRect* rect);

extern MenuSlot g_menu_slots[];

typedef struct
{
    u16 inputs[24];
} MenuScript;

extern MenuScript g_script_table[];

#define MENU_SCRIPT_END 0xFFFF

#define MENU_PAD_VERTICAL (PADLup | PADLdown)
#define MENU_PAD_SHOULDERS (PADL1 | PADL2 | PADR1 | PADR2)

/**
 * @brief Run one frame of the MENU overlay; FIELD calls it once the overlay is loaded.
 * @param render_buffers Render context of the frame.
 * @return Screen id of the request MENU hands back to FIELD, or 0 while it keeps running.
 */
s32 menu_run(RenderContext* render_buffers);

#endif
