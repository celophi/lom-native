#include "main/cd_resources.h"
#include "main/cdrom.h"
#include "main/game_state.h"
#include "lom_native_cd.h"
#include "lom_native_overlay.h"
#include "lom_native_ps1_memory.h"
#include "main/overlay_memory.h"

#include <stdint.h>
#include <stdio.h>

/**
 * @brief Stream an overlay into place and check it's the one expected.
 * @param resource_id The overlay's file.
 * @param expected_kind The overlay it should be.
 * @param info Set to which it is.
 * @return 0, or -1.
 */
static int load_and_identify(s32 resource_id, LomNativeOverlayKind expected_kind, LomNativeOverlayInfo* info)
{
    s32 decompressed_size;
    u8* destination;

    destination = (u8*)get_overlay_load_base();
    decompressed_size = cdrom_stream(resource_id, destination);

    if (decompressed_size < 0)
    {
        fprintf(stderr, "overlay resource %d failed to stream (error %d).\n", resource_id, (int)lom_native_cd_get_last_error());
        return -1;
    }

    if (lom_native_overlay_identify(resource_id, destination, (size_t)decompressed_size, info) != 0)
    {
        fprintf(stderr, "overlay resource %d was not recognized.\n", resource_id);
        return -1;
    }

    if (info->kind != expected_kind)
    {
        fprintf(stderr, "overlay resource %d resolved to unexpected kind %d.\n", resource_id, (int)info->kind);
        return -1;
    }

    printf("  %-11s id=%d bytes=%d destination=%p\n", info->name, info->overlay_id, decompressed_size, (void*)destination);
    return 0;
}

int main(int argc, char** argv)
{
    LomNativeOverlayInfo info;
    LomNativeOverlayDispatchStatus status;
    s32 result;

    if (argc > 2)
    {
        fprintf(stderr, "usage: %s [extracted-disc-root]\n", argv[0]);
        return 2;
    }

    if (lom_native_map_ps1_ram() != 0)
    {
        return 1;
    }

    if (argc == 2 && lom_native_cd_set_disc_root(argv[1]) != 0)
    {
        fprintf(stderr, "Invalid extracted-disc root: %s\n", argv[1]);
        return 2;
    }

    cdrom_init();

    puts("LoM native overlay dispatch smoke:");

    if (load_and_identify(CD_RES_CHECKPS_BIN, LOM_NATIVE_OVERLAY_CHECKPS, &info) != 0)
    {
        return 1;
    }

    status = lom_native_overlay_run(&info, (intptr_t)0x80100000u, &result);

    if (status != LOM_NATIVE_OVERLAY_DISPATCH_OK || result != GAME_STATE_INTRO_MOVIE)
    {
        fprintf(stderr, "CHECKPS native dispatch returned status=%d result=%d.\n", (int)status, result);
        return 1;
    }

    if (load_and_identify(CD_RES_TITLE_BIN, LOM_NATIVE_OVERLAY_TITLE, &info) != 0)
    {
        return 1;
    }

    status = lom_native_overlay_run(&info, (intptr_t)get_overlay_load_base(), &result);

    if (status != LOM_NATIVE_OVERLAY_DISPATCH_UNSUPPORTED)
    {
        fprintf(stderr, "TITLE native dispatch unexpectedly returned status=%d.\n", (int)status);
        return 1;
    }

    if (load_and_identify(CD_RES_WSEL_BIN, LOM_NATIVE_OVERLAY_WSEL, &info) != 0 ||
        lom_native_overlay_run(&info, (intptr_t)0x80170000u, &result) != LOM_NATIVE_OVERLAY_DISPATCH_UNSUPPORTED)
    {
        fprintf(stderr, "WSEL identity or unregistered dispatch failed.\n");
        return 1;
    }

    if (lom_native_overlay_identify(CD_RES_TITLE_BIN, get_overlay_load_base(), 4, &info) == 0 ||
        lom_native_overlay_identify(CD_RES_WSEL_BIN, get_overlay_load_base(), 3, &info) == 0)
    {
        fprintf(stderr, "WSEL accepted the wrong resource ID or a truncated header.\n");
        return 1;
    }

    puts("LoM native overlay dispatch smoke passed.");
    puts("  CHECKPS: recognized and mapped to native success-path bypass");
    puts("  TITLE:   recognized; no entry registered in this lower-level smoke");
    puts("  WSEL:    recognized; unregistered dispatch and invalid headers rejected");
    return 0;
}
