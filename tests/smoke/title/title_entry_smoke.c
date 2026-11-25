#include "lom_native_akao.h"
#include "lom_native_cd.h"
#include "lom_native_gp0_hash.h"
#include "lom_native_title_test.h"

#include "main/cd_resources.h"
#include "main/cdrom.h"
#include "main/controller.h"
#include "main/game_state.h"

#include "lom_native_gpu.h"
#include "lom_native_unit_decode.h"
#include "overlays/title/title.h"

#include <stdint.h>
#include <stdio.h>

#define TITLE_MENU_BUFFERS_PS1 0x801026D0u
#define TITLE_LOAD_PS1 0x8004FC70u
#define TITLE_DECOMPRESSED_SIZE 731740

extern u32 g_previous_game_state;

/**
 * @brief Stream TITLE into the overlay slot.
 * @param size Set to its size.
 * @return 0, or -1 when it isn't the size it should be.
 */
static int load_title(s32* size)
{
    unsigned char* window = (unsigned char*)OVERLAY_LOAD_BASE_ADDRESS;
    *size = cdrom_stream(CD_RES_TITLE_BIN, window);
    return *size == TITLE_DECOMPRESSED_SIZE ? 0 : -1;
}

/**
 * @brief Start the GPU.
 * @return What lom_native_gpu_init returns.
 */
static int start_gpu(void)
{
    return lom_native_gpu_init();
}

/**
 * @brief Run TITLE until it leaves.
 * @param result Set to the state it left for.
 * @return 0.
 */
static int run(s32* result)
{
    *result = run_title((TitleMenuContext*)TITLE_OVERLAY_END_ADDRESS);
    return 0;
}

int main(int argc, char** argv)
{
    s32 size;
    s32 result;

    if (argc != 2)
    {
        printf("usage: %s <extracted-disc-root>\n", argv[0]);
        return 2;
    }

    if (lom_native_cd_set_disc_root(argv[1]) != 0)
    {
        printf("Invalid extracted-disc root: %s\n", argv[1]);
        return 11;
    }

    cdrom_init();

    if (cdrom_get_resource_size(CD_RES_SOUND_EFFECT_SET) <= 0 || cdrom_get_resource_size(CD_RES_MSC_DATA) <= 0)
    {
        printf("TITLE entry smoke requires SOUND/EFFECT.SET and SOUND/MSC_DATA.DAT.\n");
        return 12;
    }

    if (load_title(&size) != 0)
    {
        printf("TITLE.BIN didn't load: %d bytes\n", size);
        return 13;
    }

    if (start_gpu() != 0)
    {
        return 15;
    }

    initialize_controllers(0);
    initialize_controller_vsync();

    g_previous_game_state = GAME_STATE_NONE;

    if (lom_native_akao_initialize(argv[1]) != 0)
    {
        printf("AKAO/SPU initialization failed.\n");
        return 16;
    }

    lom_native_title_test_reset();
    lom_native_title_test_set_idle_countdown(2);
    lom_native_gp0_hash_reset();

    printf("LoM TITLE entry smoke initialized.\n");
    printf("  TITLE.BIN: %d decompressed bytes\n", size);
    printf("  entry:     the game's run_title()\n");
    printf("  context:   0x%08X (TITLE's menu buffers)\n", TITLE_MENU_BUFFERS_PS1);
    printf("  exit:      TITLE's own idle timeout\n");
    printf("  audio:     the game's AKAO through PSY-Z's SPU\n");

    result = -1;
    int status = run(&result);

    lom_native_akao_shutdown();
    reset_controller_vsync_state();
    shutdown_controllers();

    if (status != 0)
    {
        printf("TITLE didn't run.\n");
        return 16;
    }

    if (result != GAME_STATE_INTRO_MOVIE)
    {
        printf("Unexpected run_title result: %d\n", result);
        return 17;
    }

    if (cdrom_get_error_status() != 0)
    {
        printf("TITLE resource loading reported a CD error.\n");
        return 19;
    }

    printf("LoM TITLE entry smoke passed.\n");
    printf("  result:       GAME_STATE_INTRO_MOVIE (%d)\n", result);
    printf("  display envs: 0x%X\n", lom_native_title_test_display_envs());
    printf("  draw envs:    0x%X\n", lom_native_title_test_draw_envs());
    printf("  submitted OTs:%d\n", lom_native_title_test_frames());
    printf("  AKAO ticks:   %u\n", lom_native_akao_tick_count());
    printf("  GP0:          %u commands, hash %08X\n", lom_native_gp0_commands(), lom_native_gp0_hash());
    return 0;
}
