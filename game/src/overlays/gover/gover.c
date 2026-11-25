#include "common.h"
#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include "lom_native_sound.h"
#include "lom_native_time.h"
#include "lom_native_draw_labels.h"
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"

#include "common/gpu_packet.h"
#include "common/pad.h"
#include "common/tim.h"
#include "internal/gover.h"
#include "main/audio/akao.h"
#include "main/audio/akao_cmd.h"
#include "main/cdrom.h"
#include "main/controller.h"
#include "main/display.h"
#include "main/game_state.h"
#include "main/main.h"
#include "overlays/field/field_input.h"
#include "overlays/field/field_sound.h"
#include "overlays/menu/menu.h"
#include "lom_native_frames.h"

typedef struct
{
    u32 active_table_offset;
    u32 reserved_0;
    u32 reserved_1;
    u8 table_data[1];
} SfxTableBuffer;

#define GOVER_OTAG_LENGTH 8

#define GOVER_FRAME_COUNT 2

typedef struct GoverFrameHalf
{
    OT_TYPE ordering_table[GOVER_OTAG_LENGTH];
    DISPENV display_environment;
    DRAWENV draw_environment;
    RECT vram_rect;
    u8 primitive_buffer[0x400];
    void* allocation_cursor;
} GoverFrameHalf;

extern s32 g_akao_music_volume;

extern SfxTableBuffer g_sfx_table_buffer;

#define GOVER_FRAME_VRAM_RECT_OFFSET (sizeof(u_long[GOVER_OTAG_LENGTH]) + sizeof(DISPENV) + sizeof(DRAWENV))

#define GOVER_FRAME_FROM_TAIL(tail, index) (((GoverFrameHalf*)((tail) - GOVER_FRAME_VRAM_RECT_OFFSET))[index])

#define GOVER_CLUT_Y 480

#define GOVER_IMAGE_HEIGHT 224

#define GOVER_TEXTURE_PAGE_WIDTH 256

#define GOVER_SECOND_TPAGE_X (SCREEN_WIDTH + 128)

#define GOVER_SFX_RESOURCE_BASE 81

#define GOVER_SFX_BANK_REUSE (-2)

#define GOVER_SFX_DISABLED (-1)

#define GOVER_MUSIC_DISABLED (-1)

#define GOVER_SFX_LOAD_BUFFER ((u8*)LOAD_BUFFER_ADDRESS)

#define GOVER_SFX_TABLE_DATA_OFFSET 0xC

#define GOVER_IMAGE_RESOURCE_BASE 0xFFC

#define GOVER_FADE_FULL 0x80

#define GOVER_FADE_STEP 4

#define GOVER_MUSIC_FADE_OUT_DURATION 0x20

#define GOVER_SFX_TABLE_OFFSET (*(u32*)LOAD_BUFFER_AT(0x4))

s32 g_gover_unused_a;

s32 g_fade_step;

s32 g_gover_unused_b;

#if defined(LOM_NATIVE_POINTERS)
static GoverFrameHalf s_gover_frames_storage[GOVER_FRAME_COUNT];
#undef GOVER_FRAME_VRAM_RECT_OFFSET
#define GOVER_FRAME_VRAM_RECT_OFFSET __builtin_offsetof(GoverFrameHalf, vram_rect)
#define g_gover_frame_header ((u8*)s_gover_frames_storage)
#define g_gover_frame_tail ((u8*)s_gover_frames_storage + GOVER_FRAME_VRAM_RECT_OFFSET)
#else
u8 g_gover_frame_header[GOVER_FRAME_VRAM_RECT_OFFSET];
u8 g_gover_frame_tail[sizeof(GoverFrameHalf) * GOVER_FRAME_COUNT - GOVER_FRAME_VRAM_RECT_OFFSET];
#endif
s32 g_fade_level;

#define g_gover_frames ((GoverFrameHalf*)g_gover_frame_header)

static void gover_load_sfx_bank(s32 sfx_bank_index);
static u32 gover_upload_image_to_vram(Tim* tim, TimUploadDestination* destinations);
static void gover_load_image_from_cd(s32 resource_index, TimUploadDestination* destinations, Tim* image_buffer);
static void gover_build_otag(u8* frame_buffer);
static void gover_run(void);

/**
 * @brief Initializes and presents the Game Over screen.
 *
 * Configures double-buffered rendering, loads the artwork and optional audio,
 * then runs the screen until fade-out completes.
 *
 * @param image_buffer Writable staging buffer for the TIM resource.
 * @param image_index Image index relative to @c GOVER_IMAGE_RESOURCE_BASE.
 * @param music_index Music index, or @c GOVER_MUSIC_DISABLED.
 * @param sfx_bank_index SFX bank index, @c GOVER_SFX_DISABLED, or
 *                       @c GOVER_SFX_BANK_REUSE.
 * @see https://decomp.me/scratch/1qYnn (100% match)
 */
void gover_show_screen(Tim* image_buffer, s32 image_index, s32 music_index, s32 sfx_bank_index)
{
    RECT vram_rect;
    u8* frame_tail;
    RECT* back_vram_rect;
    GoverFrameHalf* frames;

    lom_time_wait_menu(0);
    lom_graphics_wait(0);
    frame_tail = g_gover_frame_tail;

    lom_gpu_set_rect(&GOVER_FRAME_FROM_TAIL(frame_tail, 0).vram_rect, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    back_vram_rect = &GOVER_FRAME_FROM_TAIL(frame_tail, 1).vram_rect;
    lom_gpu_set_rect(back_vram_rect, 0, VRAM_BACK_DISP_Y, SCREEN_WIDTH, SCREEN_HEIGHT);

    lom_gpu_set_rect(&vram_rect, 0, 0, VRAM_WIDTH, VRAM_HEIGHT);
    lom_graphics_fill(&vram_rect, 0, 0, 0);

    lom_graphics_default_display(&GOVER_FRAME_FROM_TAIL(frame_tail, 0).display_environment, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    lom_graphics_default_display(&GOVER_FRAME_FROM_TAIL(frame_tail, 1).display_environment, 0, VRAM_BACK_DISP_Y, SCREEN_WIDTH, SCREEN_HEIGHT);
    lom_graphics_default_drawing(&GOVER_FRAME_FROM_TAIL(frame_tail, 0).draw_environment, 0, SCREEN_HEIGHT, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    lom_graphics_default_drawing(&GOVER_FRAME_FROM_TAIL(frame_tail, 1).draw_environment, 0, VRAM_BACK_DRAW_Y, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);

    frames = &GOVER_FRAME_FROM_TAIL(frame_tail, 0);
    frames[1].draw_environment.dtd = 0;
    frames[0].draw_environment.dtd = 0;

    vram_rect.x = SCREEN_WIDTH;
    vram_rect.y = 0;
    vram_rect.w = 0;
    vram_rect.h = GOVER_CLUT_Y;

    gover_load_image_from_cd(image_index + GOVER_IMAGE_RESOURCE_BASE, (TimUploadDestination*)&vram_rect, image_buffer);

    lom_sound_stop_all_songs();
    lom_sound_release_all_effects();
    lom_sound_set_all_effects_volume(AKAO_VOLUME_MAX);

    if (sfx_bank_index != GOVER_SFX_DISABLED)
    {
        gover_load_sfx_bank(sfx_bank_index);
        field_play_set_sfx(0, AKAO_PAN_CENTER, 0, 0);
    }

    if (music_index != GOVER_MUSIC_DISABLED)
    {
        field_load_song(music_index, 0);
        g_akao_music_volume = AKAO_VOLUME_MAX;
        field_play_song();
        lom_sound_set_song_volume(0, AKAO_VOLUME_MAX);
    }

    g_fade_level = GOVER_FADE_STEP;
    g_fade_step = GOVER_FADE_STEP;
    gover_run();
}

/**
 * @brief Runs the Game Over rendering and input loop.
 *
 * Builds alternating frames until dismissal triggers a completed fade-out.
 *
 * @see https://decomp.me/scratch/IfwJm (100% match)
 */
static void gover_run(void)
{
    GoverFrameHalf* current_frame;
    GoverFrameHalf* drawing_frame;
    GoverFrameHalf* next_frame;
    u8 stack_padding[8];

    field_reset_input_repeat();
    current_frame = g_gover_frames;
    lom_native_gover_ClearOTagR(current_frame->ordering_table, GOVER_OTAG_LENGTH);
    lom_native_gover_ClearOTagR(current_frame[1].ordering_table, GOVER_OTAG_LENGTH);
    lom_time_wait_menu(0);
    lom_graphics_set_display(&current_frame->display_environment);
    update_controllers();
    lom_graphics_show(1);

    while (1)
    {
        drawing_frame = current_frame;
        lom_native_gover_ClearOTagR(drawing_frame->ordering_table, GOVER_OTAG_LENGTH);
        drawing_frame->allocation_cursor = drawing_frame->primitive_buffer;
        field_update_input_repeat();
        gover_build_otag((u8*)drawing_frame);
        lom_native_draw_label_part("GOVER", 0, drawing_frame->primitive_buffer, drawing_frame->allocation_cursor);
        lom_graphics_wait(0);
        set_controller_vsync_interval(FRAME_VSYNC_INTERVAL);

        lom_time_wait_menu(FRAME_VSYNC_INTERVAL);

        if ((g_fade_level == GOVER_FADE_FULL) && (g_pad_input & PAD_DISMISS_BUTTONS))
        {
            lom_sound_fade_song_volume(0, GOVER_MUSIC_FADE_OUT_DURATION, 0);
            g_fade_step = -GOVER_FADE_STEP;
        }

        if (g_fade_level == 0)
        {
            break;
        }

        next_frame = g_gover_frames;

        if (current_frame == g_gover_frames)
        {
            next_frame = current_frame + 1;
        }

        current_frame = next_frame;
        lom_graphics_set_display(&current_frame->display_environment);

        lom_native_gover_PutDrawEnv(&current_frame->draw_environment);
        lom_native_gover_DrawOTag(&drawing_frame->ordering_table[GOVER_OTAG_LENGTH - 1]);
        update_controllers();
        cdrom_process_state();
    }

    lom_graphics_wait(0);
    lom_time_wait_menu(0);
    reset_controller_vsync_state();
    lom_sound_stop_all_songs();
    lom_sound_release_all_effects();
    lom_graphics_show(0);
    g_field_entry_scene.word = 0;
    field_reset_input_repeat();
    g_pending_game_state = 1;
}

/**
 * @brief Builds the GPU packets for one Game Over frame.
 *
 * Splits the artwork across two texture pages and modulates both sprites with
 * the current fade level.
 *
 * @param frame_buffer Frame buffer whose ordering table and packet workspace are populated.
 * @see https://decomp.me/scratch/q3LKi (100% match)
 */
static void gover_build_otag(u8* frame_buffer)
{
    GoverFrameHalf* frame;
    u8* primitive_a;
    u8* primitive_b;
    u8 left_fade_level;
    u8 right_fade_level;

    if (g_fade_step != 0)
    {
        g_fade_level += g_fade_step;
    }

    if (g_fade_level == GOVER_FADE_FULL)
    {
        g_fade_step = 0;
    }

    frame = (GoverFrameHalf*)frame_buffer;
    primitive_a = frame->allocation_cursor;

    lom_gpu_init_sprite(primitive_a);
    left_fade_level = g_fade_level;

    lom_gpu_set_xy0((SPRT*)primitive_a, 0, 0);
    lom_gpu_set_wh((SPRT*)primitive_a, GOVER_TEXTURE_PAGE_WIDTH, GOVER_IMAGE_HEIGHT);
    lom_gpu_set_uv0((SPRT*)primitive_a, 0, 0);
    lom_gpu_set_clut((SPRT*)primitive_a, 0, GOVER_CLUT_Y);
    SET_BGR0((SPRT*)primitive_a, left_fade_level, left_fade_level, left_fade_level);
    lom_gpu_link(frame_buffer, primitive_a);

    primitive_a += sizeof(SPRT);

    lom_gpu_init_tpage_packet((DR_TPAGE*)primitive_a, 0, 0, lom_gpu_tpage(GPU_TEXTURE_8BIT, GPU_BLEND_ADD, SCREEN_WIDTH, 0));
    lom_gpu_link(frame_buffer, primitive_a);

    primitive_b = primitive_a + sizeof(DR_TPAGE);
    primitive_a = primitive_b;

    lom_gpu_init_sprite(primitive_b);
    right_fade_level = g_fade_level;

    SET_BGR0((SPRT*)primitive_b, right_fade_level, right_fade_level, right_fade_level);
    lom_gpu_set_xy0((SPRT*)primitive_b, GOVER_TEXTURE_PAGE_WIDTH, 0);
    lom_gpu_set_wh((SPRT*)primitive_b, SCREEN_WIDTH - GOVER_TEXTURE_PAGE_WIDTH, GOVER_IMAGE_HEIGHT);
    lom_gpu_set_uv0((SPRT*)primitive_b, 0, 0);
    lom_gpu_set_clut((SPRT*)primitive_b, 0, GOVER_CLUT_Y);
    lom_gpu_link(frame_buffer, primitive_b);

    primitive_a += sizeof(SPRT);

    lom_gpu_init_tpage_packet((DR_TPAGE*)primitive_a, 0, 0, lom_gpu_tpage(GPU_TEXTURE_8BIT, GPU_BLEND_ADD, GOVER_SECOND_TPAGE_X, 0));
    primitive_b = primitive_a;
    primitive_b += sizeof(DR_TPAGE);

    lom_gpu_link(frame_buffer, primitive_a);
    frame->allocation_cursor = primitive_b;
}

/**
 * @brief Loads a TIM resource from CD and uploads it to VRAM.
 *
 * @param resource_index CD resource identifier.
 * @param destinations VRAM destinations for the TIM blocks.
 * @param image_buffer Writable staging buffer for the TIM resource.
 * @see https://decomp.me/scratch/OafFK (100% match)
 */
static void gover_load_image_from_cd(s32 resource_index, TimUploadDestination* destinations, Tim* image_buffer)
{
    u8 padding[8];
    cdrom_queue_read((u16)resource_index, image_buffer);
    cdrom_wait_queue_empty();
    gover_upload_image_to_vram(image_buffer, destinations);
}

/**
 * @brief Uploads the palette and pixel blocks of a staged 8bpp TIM.
 *
 * @param tim Staged TIM resource.
 * @param destinations VRAM destinations for the palette and pixel data.
 * @return Pixel-block width in 16-bit VRAM words, rounded up to a 64-word
 *         texture-page boundary.
 * @see https://decomp.me/scratch/BEM7D (100% match)
 */
static u32 gover_upload_image_to_vram(Tim* tim, TimUploadDestination* destinations)
{
    RECT upload_rect;
    TimBlock* pixel_block;
    u32 clut_block_length = tim->clut_block.bnum;

    lom_gpu_set_rect(&upload_rect, destinations->clut_x, destinations->clut_y, tim->clut_block.dimensions.width * tim->clut_block.dimensions.height, 1);
    lom_graphics_upload(&upload_rect, (u_long*)tim->clut_data);

    pixel_block = TIM_PIXEL_BLOCK(tim, clut_block_length);

    lom_gpu_set_rect(&upload_rect, destinations->pixel_x, destinations->pixel_y, pixel_block->dimensions.width, pixel_block->dimensions.height);
    lom_graphics_upload(&upload_rect, (u_long*)(pixel_block + 1));

    return ALIGN64(pixel_block->dimensions.width);
}

/**
 * @brief Loads and registers a Game Over SFX bank with the AKAO driver.
 *
 * Clears the previous table, copies the selected table into driver storage,
 * and uploads the accompanying AKAO instrument bank.
 *
 * @param sfx_bank_index Bank index, @c GOVER_SFX_DISABLED to clear the staged
 *                       bank, or @c GOVER_SFX_BANK_REUSE to keep it unchanged.
 * @see https://decomp.me/scratch/G5r92 (100% match)
 */
static void gover_load_sfx_bank(s32 sfx_bank_index)
{
    AkaoBankHeader* akao_bank;
    u8* copy_destination;
    u8* copy_source;
    s32* offsets;

    if (sfx_bank_index == GOVER_SFX_BANK_REUSE)
    {
        return;
    }

    g_sfx_table_buffer.reserved_1 = 0;
    g_sfx_table_buffer.reserved_0 = 0;
    g_sfx_table_buffer.active_table_offset = 0;

    if (sfx_bank_index == GOVER_SFX_DISABLED)
    {
        return;
    }

    cdrom_queue_read((u16)(sfx_bank_index + GOVER_SFX_RESOURCE_BASE), GOVER_SFX_LOAD_BUFFER);
    cdrom_wait_queue_empty();

    g_sfx_table_buffer.active_table_offset = GOVER_SFX_TABLE_DATA_OFFSET;
    copy_source = GOVER_SFX_LOAD_BUFFER + GOVER_SFX_TABLE_OFFSET;
    offsets = (s32*)copy_source;
    akao_bank = (AkaoBankHeader*)(copy_source + (u32)offsets[*offsets]);
    copy_destination = g_sfx_table_buffer.table_data;

    while (copy_source != (u8*)akao_bank)
    {
        *copy_destination++ = *copy_source++;
    }

    lom_sound_upload_bank(akao_bank, 1);
}
