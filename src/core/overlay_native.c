#include "lom_native_overlay.h"
#include "lom_native_cd.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_gname.h"
#include "overlays/gname/gname.h"

#include "main/cd_resources.h"
#include "main/game_state.h"
#include "common/saved_game.h"

#include "lom_native_game.h"
#include "lom_native_game_loop.h"
#include "lom_native_overlay_entry.h"
#include "lom_native_movie.h"
#include "lom_native_field.h"
#include "lom_native_wmap.h"
#include "lom_native_wsel.h"
#include "lom_native_gpu.h"
#include "lom_native_mods.h"
#include "overlays/gname/internal/overlay_header.h"
#include "overlays/title/internal/overlay_header.h"
#include "overlays/wsel/internal/overlay_header.h"
#include <stdio.h>

#define g_checkps_overlay_id 17
#define SECONDARY_OVERLAY_ADDRESS_PS1 0x80140000u

static LomNativeWselEntry s_wsel_entry;

static LomNativeTitleEntry s_title_entry;

static LomNativeGnameEntry s_gname_entry;

static int s_game_started;

static unsigned s_started_game_id;

/**
 * @brief Name an overlay the port runs, by its file and the ID at its start.
 * @param resource_id The overlay's file.
 * @param overlay_id The ID it was loaded with.
 * @param info Set to which overlay it is.
 * @return 0, or -1 when it isn't one of them or its ID doesn't match.
 */
static int fill_overlay_info(s32 resource_id, s32 overlay_id, LomNativeOverlayInfo* info)
{
    const struct
    {
        s32 resource_id;
        s32 overlay_id;
        LomNativeOverlayKind kind;
        const char* name;
    } overlays[] = {
        {CD_RES_CHECKPS_BIN, g_checkps_overlay_id, LOM_NATIVE_OVERLAY_CHECKPS, "CHECKPS.BIN"},
        {CD_RES_TITLE_BIN, g_title_overlay_id, LOM_NATIVE_OVERLAY_TITLE, "TITLE.BIN"},
        {CD_RES_GNAME_BIN, g_gname_overlay_id, LOM_NATIVE_OVERLAY_GNAME, "GNAME.BIN"},
        {CD_RES_WSEL_BIN, g_wsel_overlay_id, LOM_NATIVE_OVERLAY_WSEL, "WSEL.BIN"},
    };

    for (size_t i = 0; i < sizeof(overlays) / sizeof(overlays[0]); ++i)
    {
        if (resource_id == overlays[i].resource_id && overlay_id == overlays[i].overlay_id)
        {
            info->kind = overlays[i].kind;
            info->resource_id = resource_id;
            info->overlay_id = overlay_id;
            info->name = overlays[i].name;
            return 0;
        }
    }

    return -1;
}

void lom_native_overlay_set_wsel_entry(LomNativeWselEntry entry)
{
    s_wsel_entry = entry;
}

void lom_native_overlay_set_title_entry(LomNativeTitleEntry entry)
{
    s_title_entry = entry;
}

void lom_native_overlay_set_gname_entry(LomNativeGnameEntry entry)
{
    s_gname_entry = entry;
}

int lom_native_overlay_identify(s32 resource_id, const void* image, size_t image_size, LomNativeOverlayInfo* info)
{
    const s32* words;

    if (image == NULL || info == NULL || image_size < sizeof(s32))
    {
        return -1;
    }

    words = (const s32*)image;
    return fill_overlay_info(resource_id, words[0], info);
}

LomNativeOverlayDispatchStatus lom_native_overlay_run(const LomNativeOverlayInfo* info, intptr_t arg0, s32* result)
{
    (void)arg0;

    if (info == NULL || result == NULL)
    {
        return LOM_NATIVE_OVERLAY_DISPATCH_INVALID_IMAGE;
    }

    switch (info->kind)
    {
    case LOM_NATIVE_OVERLAY_CHECKPS:
        fprintf(stderr, "[lom-native] Skipping CHECKPS's console check.\n");
        *result = GAME_STATE_INTRO_MOVIE;
        return LOM_NATIVE_OVERLAY_DISPATCH_OK;

    case LOM_NATIVE_OVERLAY_WSEL:
        if (s_wsel_entry == NULL)
        {
            return LOM_NATIVE_OVERLAY_DISPATCH_UNSUPPORTED;
        }

        *result = s_wsel_entry(arg0);
        return LOM_NATIVE_OVERLAY_DISPATCH_OK;

    case LOM_NATIVE_OVERLAY_TITLE:
        if (s_title_entry == NULL)
        {
            return LOM_NATIVE_OVERLAY_DISPATCH_UNSUPPORTED;
        }

        *result = s_title_entry(arg0);
        return LOM_NATIVE_OVERLAY_DISPATCH_OK;

    default:
        return LOM_NATIVE_OVERLAY_DISPATCH_UNSUPPORTED;
    }
}

LomNativeOverlayDispatchStatus lom_native_overlay_run_gname(const LomNativeOverlayInfo* info, intptr_t render_buffers, intptr_t initial_name,
                                                            intptr_t active_name, s32 source_mode, s32 history_index, intptr_t custom_name,
                                                            s32 allow_empty_cancel, s32* result)
{
    if (info == NULL || result == NULL)
    {
        return LOM_NATIVE_OVERLAY_DISPATCH_INVALID_IMAGE;
    }

    if (info->kind != LOM_NATIVE_OVERLAY_GNAME || s_gname_entry == NULL)
    {
        return LOM_NATIVE_OVERLAY_DISPATCH_UNSUPPORTED;
    }

    *result = s_gname_entry(render_buffers, initial_name, active_name, source_mode, history_index, custom_name, allow_empty_cancel);
    return LOM_NATIVE_OVERLAY_DISPATCH_OK;
}

static const char* s_last_name = "";

const char* lom_native_overlay_last_name(void)
{
    return s_last_name;
}

void lom_overlay_require(unsigned int id, const char* name)
{
    s_last_name = name;

    if (lom_native_cd_get_last_error() != LOM_NATIVE_CD_ERROR_NONE || *(u32*)LOM_PS1_ADDRESS(0x80140000u) != id)
    {
        char reason[96];
        snprintf(reason, sizeof(reason), "%s overlay unavailable or invalid", name);
        lom_native_field_fail(reason);
    }
}

void lom_overlay_run_name_entry(unsigned char* initial, unsigned char* active, int source, int history, const unsigned char* custom)
{
    lom_native_cd_stream(CD_RES_GNAME_BIN, (void*)SECONDARY_OVERLAY_ADDRESS);
    lom_native_cd_wait();
    lom_overlay_require(5, "GNAME");
    lom_native_run_gname((intptr_t)GNAME_RENDER_BUFFERS, (intptr_t)initial, (intptr_t)active, source, history, (intptr_t)custom, 0);
}

int lom_overlay_load(int resource, void* address)
{
    s32 size = lom_native_cd_stream(resource, address);

    if (size < 0)
    {
        fprintf(stderr, "[lom-native] Failed to stream resource %d (CD error %d).\n", resource, (int)lom_native_cd_get_last_error());
        lom_native_game_stop(LOM_NATIVE_GAME_ERROR);
    }

    return size;
}

void lom_overlay_wait(void)
{
    lom_native_cd_wait();
}

void lom_overlay_check(const void* image, unsigned int id, const char* name)
{
    if (lom_native_cd_get_last_error() != LOM_NATIVE_CD_ERROR_NONE || *(const u32*)image != id)
    {
        fprintf(stderr, "[lom-native] %s overlay unavailable or invalid.\n", name);
        lom_native_game_stop(LOM_NATIVE_GAME_ERROR);
    }
}

void lom_overlay_check_reads(void)
{
    if (lom_native_cd_get_last_error() != LOM_NATIVE_CD_ERROR_NONE)
    {
        lom_native_game_stop(LOM_NATIVE_GAME_ERROR);
    }
}

void lom_overlay_stop_if_quitting(void)
{
    if (lom_native_gpu_quit_requested())
    {
        lom_native_game_stop(LOM_NATIVE_GAME_QUIT);
    }
}

/**
 * @brief Run an overlay that's loaded, after checking it's the one expected.
 *        The game stops when it isn't or can't be run.
 * @param resource Its file.
 * @param kind The overlay expected.
 * @param image Where it's loaded.
 * @param argument What its entry takes.
 * @return What its entry returns.
 */
static int run_loaded(s32 resource, LomNativeOverlayKind kind, void* image, intptr_t argument)
{
    LomNativeOverlayInfo info;
    s32 result = -1;

    if (lom_native_overlay_identify(resource, image, sizeof(s32), &info) != 0 || info.kind != kind)
    {
        fprintf(stderr, "[lom-native] Overlay resource %d isn't the overlay the game expected.\n", (int)resource);
        lom_native_game_stop(LOM_NATIVE_GAME_ERROR);
    }

    if (lom_native_overlay_run(&info, argument, &result) != LOM_NATIVE_OVERLAY_DISPATCH_OK)
    {
        fprintf(stderr, "[lom-native] %s native dispatch failed.\n", info.name);
        lom_native_game_stop(LOM_NATIVE_GAME_ERROR);
    }

    return result;
}

int lom_overlay_run_checkps(void* image, void* render)
{
    return run_loaded(CD_RES_CHECKPS_BIN, LOM_NATIVE_OVERLAY_CHECKPS, image, (intptr_t)render);
}

int lom_overlay_run_title(void* image, void* menu_buffers)
{
    return run_loaded(CD_RES_TITLE_BIN, LOM_NATIVE_OVERLAY_TITLE, image, (intptr_t)menu_buffers);
}

int lom_overlay_run_wsel(void* image, void* render)
{
    return run_loaded(CD_RES_WSEL_BIN, LOM_NATIVE_OVERLAY_WSEL, image, (intptr_t)render);
}

int lom_overlay_run_gname(void* render, unsigned char* initial, unsigned char* active, int source, int history, unsigned char* custom, int allow_empty_cancel)
{
    LomNativeOverlayInfo info;
    s32 result = -1;
    void* image = (void*)LOM_PS1_ADDRESS(SECONDARY_OVERLAY_ADDRESS_PS1);

    if (lom_native_overlay_identify(CD_RES_GNAME_BIN, image, sizeof(s32), &info) != 0 || info.kind != LOM_NATIVE_OVERLAY_GNAME ||
        lom_native_overlay_run_gname(&info, (intptr_t)render, (intptr_t)initial, (intptr_t)active, source, history, (intptr_t)custom, allow_empty_cancel,
                                     &result) != LOM_NATIVE_OVERLAY_DISPATCH_OK)
    {
        fprintf(stderr, "[lom-native] GNAME native dispatch failed.\n");
        lom_native_game_stop(LOM_NATIVE_GAME_ERROR);
    }

    return result;
}

/**
 * @brief Tell the mods a game has started, when FIELD is entered from
 *        anywhere but FIELD or the world map (a new game, a load from the
 *        title, world select), or with another game in memory than the last
 *        one started (a save from another playthrough loaded in FIELD).
 *        Coming back from the world map isn't a new start.
 */
static void start_game_for_mods(void)
{
    const unsigned game_id = g_saved_game.layout.identity.ids.game_id;
    const int returning = g_previous_game_state == GAME_STATE_FIELD || g_previous_game_state == GAME_STATE_WORLD_MAP;

    if (s_game_started && returning && s_started_game_id == game_id)
    {
        return;
    }

    uint32_t fallback = 2166136261u;
    const u8* name = g_saved_game.layout.characters[0].name;

    fallback = (fallback ^ (game_id & 0xFFu)) * 16777619u;
    fallback = (fallback ^ (game_id >> 8)) * 16777619u;

    for (size_t i = 0; i < sizeof(g_saved_game.layout.characters[0].name); ++i)
    {
        fallback = (fallback ^ name[i]) * 16777619u;
    }

    s_game_started = 1;
    s_started_game_id = game_id;
    lom_native_mods_game_start(game_id, fallback);
}

int lom_overlay_run_field(int (*enter)(void))
{
    int32_t next_state = 0;
    start_game_for_mods();
    LomNativeGameStatus status = lom_native_field_run_entry(enter, &next_state);

    if (status != LOM_NATIVE_GAME_RUNNING)
    {
        lom_native_game_stop(status);
    }

    return next_state;
}

int lom_overlay_run_world_map(int (*enter)(void))
{
    int32_t next_state = 0;
    LomNativeGameStatus status = lom_native_wmap_run_entry(enter, &next_state);

    if (status != LOM_NATIVE_GAME_RUNNING)
    {
        lom_native_game_stop(status);
    }

    return next_state;
}

void lom_overlay_play_movie(int index)
{
    if (lom_native_movie_play((unsigned)index) == LOM_NATIVE_MOVIE_ERROR)
    {
        fprintf(stderr, "[lom-native] Movie %d playback failed.\n", index);
        lom_native_game_stop(LOM_NATIVE_GAME_ERROR);
    }

    lom_overlay_stop_if_quitting();
}

void lom_overlay_unsupported_state(unsigned int state)
{
    fprintf(stderr, "[lom-native] Game state %u isn't supported yet, so the game stops here.\n", state);
    lom_native_game_stop(LOM_NATIVE_GAME_UNSUPPORTED_STATE);
}

void lom_overlay_fail(const char* reason)
{
    fprintf(stderr, "[lom-native] %s\n", reason);
    lom_native_game_stop(LOM_NATIVE_GAME_ERROR);
}
