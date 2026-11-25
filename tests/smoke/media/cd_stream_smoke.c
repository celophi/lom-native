#include "main/cdrom.h"
#include "main/cd_resources.h"
#include "common.h"

#include "lom_native_cd.h"
#include "lom_native_ps1_memory.h"
#include "overlays/title/internal/overlay_header.h"

#include <stdio.h>

#define TITLE_DESTINATION_ADDRESS 0x8004FC70u
#define TITLE_EXPECTED_DECOMPRESSED_SIZE 731740

int main(int argc, char** argv)
{
    s32 decompressed_size;
    const s32* overlay_words;

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

    decompressed_size = cdrom_stream(CD_RES_TITLE_BIN, (u8*)MEMORY_MAP_RAM(TITLE_DESTINATION_ADDRESS));

    if (decompressed_size < 0)
    {
        fprintf(stderr, "TITLE.BIN stream/decompression failed (error %d).\n", (int)lom_native_cd_get_last_error());
        return 1;
    }

    if (decompressed_size != TITLE_EXPECTED_DECOMPRESSED_SIZE)
    {
        fprintf(stderr, "TITLE.BIN decompressed to %d bytes; expected %d.\n", decompressed_size, TITLE_EXPECTED_DECOMPRESSED_SIZE);
        return 1;
    }

    overlay_words = (const s32*)MEMORY_MAP_RAM(TITLE_DESTINATION_ADDRESS);

    if (overlay_words[0] != g_title_overlay_id)
    {
        fprintf(stderr, "TITLE.BIN overlay id is %d; upstream title id is %d.\n", overlay_words[0], g_title_overlay_id);
        return 1;
    }

    printf("LoM CD stream smoke passed.\n");
    printf("  disc root:     %s\n", lom_native_cd_get_disc_root());
    printf("  resource:      %d (TITLE.BIN)\n", CD_RES_TITLE_BIN);
    printf("  compressed:    %d bytes\n", cdrom_get_resource_size(CD_RES_TITLE_BIN));
    printf("  decompressed:  %d bytes\n", decompressed_size);
    printf("  destination:   0x%08X\n", TITLE_DESTINATION_ADDRESS);
    printf("  overlay id:    %d\n", overlay_words[0]);
    return 0;
}
