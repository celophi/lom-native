#include "common.h"
#include <libgpu.h>
#include <rand.h>
#include "lom_native_sound.h"
#include "lom_native_time.h"
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"
#include "lom_native_overlay_entry.h"
#include "lom_native_game_loop.h"

#include "main/audio/akao.h"
#include "main/audio/akao_cmd.h"
#include "main/audio/game_audio.h"
#include "main/cd_resources.h"
#include "main/cdrom.h"
#include "main/controller.h"
#include "main/display.h"
#include "main/field_runtime.h"
#include "main/main.h"
#include "main/overlay_memory.h"
#include "main/screen_transition.h"
#include "overlays/checkps/checkps.h"
#include "overlays/cload/cload.h"
#include "overlays/gname/gname.h"
#include "overlays/movie/movie.h"
#include "overlays/title/title.h"
#include "overlays/wmap/world_map.h"
#include "overlays/wsel/wsel.h"

#define CD_RESOURCE_TABLE_LBA 24
#define CD_RESOURCE_TABLE_SIZE 46488
#define FIELD_FONT_LOAD_ADDRESS ((u8*)FONT_ADDRESS)

#define CHECKPS_RENDER_ADDRESS PRIMARY_OVERLAY_UPPER_ADDRESS
#define WSEL_RENDER_ADDRESS SECONDARY_OVERLAY_AT(0x30000)

#if !defined(VERSION_JP)
extern s32 g_startup_unused_a;
extern s32 g_startup_unused_b;
#endif

/**
 * @brief The game's start-up, after the hardware: the font, the random seed,
 *        the first FIELD entry, and CHECKPS.
 * @see decomp.me (100%) https://decomp.me/scratch/Tc7j3 (main_game_loop)
 */
void main_game_start(void)
{
    lom_overlay_load(CD_RES_FIELD_FONT, FIELD_FONT_LOAD_ADDRESS);
    srand(1);
    g_save_compatibility_tag = SAVE_TAG_STARTUP;
#if defined(VERSION_JP)
    g_game_state = GAME_STATE_CHECKPS;
#else
    g_startup_unused_a = 11;
    g_startup_unused_b = 19;
    g_game_state = GAME_STATE_INTRO_MOVIE;
    get_field_render_buffers();
    lom_overlay_load(CD_RES_CHECKPS_BIN, g_overlay_load_address);
    lom_overlay_wait();
    g_game_state = lom_overlay_run_checkps(g_overlay_load_address, (CheckPSRenderState*)CHECKPS_RENDER_ADDRESS);
    lom_graphics_wait(0);
    lom_time_wait(0);
#endif
    g_previous_game_state = GAME_STATE_NONE;
}

/**
 * @brief FIELD's own start: load it, play the attract movies, set where it
 *        starts, and run it until it asks for another state.
 * @return The next game state.
 * @note Native: the port runs this inside its guard for FIELD
 *       (lom_overlay_run_field), which stops it cleanly if the player quits.
 *       The songs stop before the load, not after the last state; and coming
 *       from CLOAD, the saved game's place is set here, after the load, since
 *       loading FIELD puts back its own globals.
 */
s32 main_enter_field(void)
{
    lom_graphics_show(0);
    lom_time_wait(0);
    lom_graphics_wait(0);
    lom_sound_stop_all_songs();
    lom_sound_release_all_effects();
    get_field_render_buffers();
    lom_overlay_load(CD_RES_FIELD_BIN, g_overlay_load_address);
    lom_overlay_check(g_overlay_load_address, 4, "FIELD");
    lom_overlay_wait();

    if (g_game_state == GAME_STATE_ATTRACT_1 || g_game_state == GAME_STATE_ATTRACT_2)
    {
        lom_overlay_load(CD_RES_MOVIE_BIN, SECONDARY_OVERLAY_LOAD_ADDRESS);
        lom_overlay_wait();

        if (g_game_state == GAME_STATE_ATTRACT_1)
        {
            lom_overlay_play_movie(MOVIE_INDEX_ATTRACT_1);
        }
        else
        {
            lom_overlay_play_movie(MOVIE_INDEX_ATTRACT_2_PART_1);
            lom_overlay_play_movie(MOVIE_INDEX_ATTRACT_2_PART_2);
            lom_overlay_play_movie(MOVIE_INDEX_ATTRACT_2_PART_3);
        }
    }

    if (g_game_state == GAME_STATE_MENU_LOAD)
    {
        g_saved_game.layout.spawn.bits.id = FIELD_SPAWN_LOAD_GAME;
        g_field_spawn_id = FIELD_SPAWN_LOAD_GAME;
        g_field_entry_scene.word = g_saved_game.layout.scene_id;
        g_field_object_id = g_saved_game.layout.object_id;
        g_field_music_id = g_saved_game.layout.music_id;
        g_field_sound_bank_id = g_saved_game.layout.sound_bank_id;
        g_field_secondary_music_id = g_saved_game.layout.secondary_music_id;
    }
    else
    {
        g_field_object_id = 0;
        g_field_spawn_id = 0;
    }

    return run_field_scene();
}

/**
 * @brief The world map's own start: clear the screen, stop the songs, load it
 *        and run it.
 * @return The next game state.
 * @note Native: the port runs this inside its guard for the world map
 *       (lom_overlay_run_world_map). The songs stop and the screen clears
 *       before the load: decompressing on a PC is fast enough to overwrite the
 *       sound effects program that's still fading out, which the PS1's CD never
 *       was.
 */
s32 main_enter_world_map(void)
{
    RECT rect;

    screen_transition(0);
    lom_gpu_set_rect(&rect, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT * 2);
    lom_graphics_fill(&rect, 0, 0, 0);
    lom_graphics_wait(0);
    lom_time_wait(0);
    lom_sound_stop_all_songs();
    lom_sound_release_all_effects();
    get_world_map_overlay_end();
    lom_overlay_load(CD_RES_WMAP_BIN, g_overlay_load_address);
    lom_overlay_check(g_overlay_load_address, 15, "WMAP");
    lom_overlay_wait();
    return run_world_map();
}

/**
 * @brief Run the current game state's overlay once, and set the next state.
 * @note JP starts in GAME_STATE_CHECKPS, which the PS1 also uses to call
 *       akao_init. Natively the port starts the sound driver itself
 *       (lom_native_akao_initialize), and CHECKPS only runs at boot, so the
 *       case leaves that call out.
 * @see decomp.me (100%) https://decomp.me/scratch/Tc7j3 (main_game_loop)
 */
void main_game_run_state(void)
{
    TitleMenuContext* title_menu_buffers;
    u8* player_name;

    if (g_game_state == GAME_STATE_RETURN_TO_TITLE)
    {
        g_game_state = GAME_STATE_TITLE;
    }

    switch (g_game_state)
    {
    case GAME_STATE_FIELD:
    case GAME_STATE_ATTRACT_1:
    case GAME_STATE_ATTRACT_2:
        g_game_state = lom_overlay_run_field(main_enter_field);
        lom_sound_stop_all_songs();
        lom_sound_release_all_effects();
        lom_sound_set_song_volume(0, AKAO_VOLUME_MAX);
        g_previous_game_state = GAME_STATE_FIELD;
        break;

    case GAME_STATE_WORLD_MAP:
        g_game_state = lom_overlay_run_world_map(main_enter_world_map);
        lom_sound_stop_all_songs();
        lom_sound_release_all_effects();

        if ((g_game_state != GAME_STATE_TITLE) && (g_game_state != GAME_STATE_ATTRACT_1) && (g_game_state != GAME_STATE_ATTRACT_2))
        {
            load_and_play_song(g_land_songs[g_current_land]);
            lom_overlay_check_reads();
        }

        g_field_secondary_music_id = -1;
        g_previous_game_state = GAME_STATE_WORLD_MAP;
        break;

    case GAME_STATE_TITLE:
        title_menu_buffers = get_title_menu_buffers();
        cdrom_stop();
        lom_overlay_load(CD_RES_TITLE_BIN, g_overlay_load_address);
        screen_transition(0);
        lom_overlay_wait();
        g_game_state = lom_overlay_run_title(g_overlay_load_address, title_menu_buffers);
        lom_graphics_wait(0);
        lom_time_wait(0);
        g_previous_game_state = GAME_STATE_TITLE;
        break;

    case GAME_STATE_GNAME:
        get_field_render_buffers();
        lom_overlay_load(CD_RES_FIELD_BIN, g_overlay_load_address);
        lom_overlay_load(CD_RES_GNAME_BIN, SECONDARY_OVERLAY_LOAD_ADDRESS);
        screen_transition(0);
        lom_overlay_wait();
        lom_overlay_check(SECONDARY_OVERLAY_LOAD_ADDRESS, 5, "GNAME");
        field_restore_entry_music();
        field_scene_reset(NULL);
        g_field_audio_timer = 0;
        player_name = g_saved_game.layout.characters[0].name;
        g_game_state =
            lom_overlay_run_gname(GNAME_RENDER_BUFFERS, player_name, player_name, g_saved_game.layout.characters[0].info.bits.type + 4, 0, player_name, 1);
        lom_graphics_wait(0);
        lom_time_wait(0);
        g_previous_game_state = GAME_STATE_GNAME;
        break;

    case GAME_STATE_WORLD_SELECT:
        get_field_render_buffers();
        lom_overlay_load(CD_RES_WSEL_BIN, g_overlay_load_address);
        screen_transition(0);
        lom_overlay_wait();
        g_game_state = lom_overlay_run_wsel(g_overlay_load_address, (void*)WSEL_RENDER_ADDRESS);
        lom_graphics_wait(0);
        lom_time_wait(0);
        g_previous_game_state = GAME_STATE_WORLD_SELECT;
        break;

#if defined(VERSION_JP)
    case GAME_STATE_CHECKPS:
        get_field_render_buffers();
        lom_overlay_load(CD_RES_CHECKPS_BIN, g_overlay_load_address);
        screen_transition(0);
        lom_overlay_wait();
        g_game_state = lom_overlay_run_checkps(g_overlay_load_address, (CheckPSRenderState*)CHECKPS_RENDER_ADDRESS);
        lom_graphics_wait(0);
        lom_time_wait(0);
        g_previous_game_state = GAME_STATE_CHECKPS;
        break;

#endif
    case GAME_STATE_MENU_LOAD:
        get_field_render_buffers();
        lom_overlay_load(CD_RES_FIELD_BIN, g_overlay_load_address);
        lom_overlay_check(g_overlay_load_address, 4, "FIELD");
        lom_overlay_load(CD_RES_CLOAD_BIN, SECONDARY_OVERLAY_LOAD_ADDRESS);
        screen_transition(0);
        lom_overlay_wait();
        field_scene_reset(NULL);
        g_save_compatibility_tag = SAVE_TAG_STARTUP;

        if (cload_main() != CLOAD_RESULT_LOADED)
        {
            lom_overlay_stop_if_quitting();
            g_game_state = GAME_STATE_TITLE;
        }
        else
        {
            lom_overlay_stop_if_quitting();
            g_current_land = g_saved_game.layout.location.bits.current_land;

            if ((g_saved_game.layout.options.word & (SAVED_OPTION_GAME_CLEARED | SAVED_OPTION_WORLD_SELECT_PENDING)) ==
                (SAVED_OPTION_GAME_CLEARED | SAVED_OPTION_WORLD_SELECT_PENDING))
            {
                g_game_state = GAME_STATE_WORLD_SELECT;
            }
            else
            {
                screen_transition(0);
                g_game_state = lom_overlay_run_field(main_enter_field);
                lom_sound_stop_all_songs();
                lom_sound_release_all_effects();
                lom_sound_set_song_volume(0, AKAO_VOLUME_MAX);
                g_previous_game_state = GAME_STATE_FIELD;
                break;
            }
        }

        g_previous_game_state = GAME_STATE_FIELD;
        break;

    case GAME_STATE_INTRO_MOVIE:
        get_title_menu_buffers();
        lom_overlay_load(CD_RES_MOVIE_BIN, SECONDARY_OVERLAY_LOAD_ADDRESS);
        screen_transition(0);
        lom_overlay_wait();
        lom_overlay_play_movie(MOVIE_INDEX_INTRO);
        g_game_state = GAME_STATE_TITLE;
        lom_graphics_wait(0);
        lom_time_wait(0);
        g_previous_game_state = GAME_STATE_INTRO_MOVIE;
        break;

    default:
        lom_overlay_unsupported_state(g_game_state);
    }
}

/**
 * @brief Initialize the hardware and dispatch the game's overlays forever.
 * @note Native: the port calls the parts itself (above); this is the PS1's
 *       whole, kept for reading.
 * @see decomp.me (100%) https://decomp.me/scratch/Tc7j3
 */
void main_game_loop(void)
{
    main_game_start();

    while (1)
    {
        main_game_run_state();
    }
}
