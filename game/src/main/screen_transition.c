#include "main/screen_transition.h"
#include "main/cdrom.h"
#include "main/controller.h"
#include "main/display.h"
#include <libgte.h>
#include <libgpu.h>
#include <libetc.h>
#include "common/gpu_packet.h"
#include "lom_native_time.h"
#include "lom_native_frames.h"
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"
#include "lom_native_widescreen.h"

#define TRANSITION_OT_SIZE 4
#define TRANSITION_PACKET_WORDS 64
#define TRANSITION_FRAME_COUNT 16
#define TRANSITION_FADE_LEVEL 32
#define TRANSITION_SUBTRACT_TPAGE lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_SUBTRACT, 0, 0)

typedef struct
{
    OT_TYPE ot[TRANSITION_OT_SIZE];
    u_long packets[TRANSITION_PACKET_WORDS];
    DISPENV display;
    DRAWENV draw;
    RECT display_rect;
} TransitionFrame;

#if defined(LOM_NATIVE_POINTERS)
TransitionFrame g_transition_frames[DISPLAY_BUFFER_COUNT];
#else
extern TransitionFrame g_transition_frames[DISPLAY_BUFFER_COUNT];
_Static_assert(sizeof(TransitionFrame) == 0x188, "PS1 transition frame ABI");
#endif

/**
 * @brief Fade out the screen while continuing controller and CD processing.
 * @param skip_fade Nonzero waits without fading or disabling the display.
 * @note In the port a wide frame fades wide, and the view goes back to the original after, for the next screen.
 * @see decomp.me (100%) https://decomp.me/scratch/clAOi
 */
void screen_transition(s32 skip_fade)
{
    TransitionFrame* current;
    RECT rect;
    s32 frame;
    TILE* tile;
    void* packet;

    lom_graphics_wait(0);
    lom_time_wait(0);

    if (skip_fade == 0)
    {
        lom_gpu_set_rect(&rect, 0, SCREEN_HEIGHT, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
        lom_graphics_copy(&rect, 0, VRAM_BACK_DRAW_Y);
        lom_graphics_wait(0);
    }

    lom_gpu_set_rect(&g_transition_frames[0].display_rect, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    lom_gpu_set_rect(&g_transition_frames[1].display_rect, 0, VRAM_BACK_DISP_Y, SCREEN_WIDTH, SCREEN_HEIGHT);

    lom_graphics_default_display(&g_transition_frames[0].display, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    lom_graphics_default_display(&g_transition_frames[1].display, 0, VRAM_BACK_DISP_Y, SCREEN_WIDTH, SCREEN_HEIGHT);
    lom_graphics_default_drawing(&g_transition_frames[0].draw, 0, SCREEN_HEIGHT, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    lom_graphics_default_drawing(&g_transition_frames[1].draw, 0, VRAM_BACK_DRAW_Y, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);

    g_transition_frames[1].draw.dtd = 0;
    g_transition_frames[0].draw.dtd = 0;
    lom_native_title_ClearOTagR(g_transition_frames[1].ot, TRANSITION_OT_SIZE);
    lom_native_title_PutDispEnv(&g_transition_frames[0].display);
    current = g_transition_frames;

    for (frame = 0; frame < TRANSITION_FRAME_COUNT; frame++)
    {
        lom_native_title_ClearOTagR(current->ot, TRANSITION_OT_SIZE);
        packet = current->packets;

        if (skip_fade == 0)
        {
            tile = packet;
            lom_gpu_init_tile(tile);
            tile->b0 = TRANSITION_FADE_LEVEL;
            tile->g0 = TRANSITION_FADE_LEVEL;
            tile->r0 = TRANSITION_FADE_LEVEL;
            lom_gpu_set_xy0(tile, -lom_native_widescreen_margin(), 0);
            lom_gpu_set_wh(tile, SCREEN_WIDTH + 2 * lom_native_widescreen_margin(), VRAM_DRAW_HEIGHT);
            lom_gpu_set_semi_trans(tile, 1);
            lom_gpu_link(&current->ot[0], tile);

            packet = tile + 1;
            lom_gpu_init_tpage_packet(packet, 0, 0, TRANSITION_SUBTRACT_TPAGE);
            lom_gpu_link(&current->ot[0], packet);
        }

        lom_graphics_wait(0);
        lom_time_wait(0);
        current = (current == &g_transition_frames[0]) ? &g_transition_frames[1] : &g_transition_frames[0];

        lom_native_title_PutDispEnv(&current->display);
        lom_native_title_PutDrawEnv(&current->draw);
        lom_native_title_DrawOTag(&current->ot[TRANSITION_OT_SIZE - 1]);
        update_controllers();
        cdrom_process_state();
    }

    reset_controller_vsync_state();
    lom_graphics_wait(0);
    lom_time_wait(0);

    if (skip_fade == 0)
    {
        lom_graphics_show(0);
    }

    lom_native_widescreen_reset();
}
