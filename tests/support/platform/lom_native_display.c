#include "lom_native_display.h"

#define SDK_LIBGPU_H
#define LOM_NATIVE_PSYZ_GPU_VIEW 1
#define u_long LomPs1ULong
#include "main/display.h"
#undef u_long

#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>

#define LOM_NATIVE_OT_LENGTH 4
#define LOM_NATIVE_OT_SIZE (1 << LOM_NATIVE_OT_LENGTH)

typedef struct LomNativeFrame
{
    DISPENV disp_env;

    DRAWENV draw_env;

    OT_TYPE ordering_table[LOM_NATIVE_OT_SIZE];

    TILE background;

    TILE transition_tile;

    DR_TPAGE transition_tpage;
} LomNativeFrame;

static LomNativeFrame s_frames[2];

static s32 s_current_frame;

static u32 s_frame_counter;

/**
 * @brief Initialize one half of the original LoM double-buffer display layout.
 * @param frame Frame state to initialize.
 * @param display_y Display-region Y coordinate in PS1 VRAM.
 * @param draw_y Draw-region Y coordinate in PS1 VRAM.
 */
static void lom_native_init_frame(LomNativeFrame* frame, s32 display_y, s32 draw_y)
{
    SetDefDispEnv(&frame->disp_env, 0, display_y, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDrawEnv(&frame->draw_env, 0, draw_y, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    frame->draw_env.dtd = 0;
    ClearOTagR(frame->ordering_table, LOM_NATIVE_OT_SIZE);
}

/**
 * @brief Initialize the LoM-native display state using the original PS1 layout.
 */
void lom_native_display_init(void)
{
    RECT vram_rect;

    SetGeomScreen(0x5DC);
    SetGeomOffset(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

    setRECT(&vram_rect, 0, 0, VRAM_WIDTH, VRAM_HEIGHT);
    ClearImage(&vram_rect, 0, 0, 0);
    DrawSync(0);

    lom_native_init_frame(&s_frames[0], 0, SCREEN_HEIGHT);
    lom_native_init_frame(&s_frames[1], VRAM_BACK_DISP_Y, VRAM_BACK_DRAW_Y);

    s_current_frame = 0;
    s_frame_counter = 0;

    PutDispEnv(&s_frames[0].disp_env);
    PutDrawEnv(&s_frames[0].draw_env);
    SetDispMask(1);
}

/**
 * @brief Build the LoM transition packet sequence in a native-safe packet buffer.
 * @param frame Frame whose ordering table receives the packets.
 */
static void lom_native_build_transition_packets(LomNativeFrame* frame)
{
    u8 pulse;

    setTile(&frame->transition_tile);
    setRGB0(&frame->transition_tile, 32, 32, 32);
    setXY0(&frame->transition_tile, 0, 0);
    setWH(&frame->transition_tile, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    setSemiTrans(&frame->transition_tile, 1);
    addPrim(&frame->ordering_table[0], &frame->transition_tile);

    setDrawTPage(&frame->transition_tpage, 0, 0, 0x40);
    addPrim(&frame->ordering_table[0], &frame->transition_tpage);

    pulse = (u8)(48 + ((s_frame_counter >> 1) & 0x3F));
    setTile(&frame->background);
    setRGB0(&frame->background, 24, pulse, 96);
    setXY0(&frame->background, 0, 0);
    setWH(&frame->background, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    addPrim(&frame->ordering_table[0], &frame->background);
}

/**
 * @brief Render and present one LoM-derived bootstrap frame.
 */
void lom_native_display_present(void)
{
    LomNativeFrame* frame;

    frame = &s_frames[s_current_frame];
    ClearOTagR(frame->ordering_table, LOM_NATIVE_OT_SIZE);
    lom_native_build_transition_packets(frame);

    DrawSync(0);
    VSync(0);
    PutDispEnv(&frame->disp_env);
    PutDrawEnv(&frame->draw_env);
    DrawOTag(&frame->ordering_table[LOM_NATIVE_OT_SIZE - 1]);

    s_current_frame ^= 1;
    s_frame_counter++;
}
