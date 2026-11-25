#include "lom_native_cd.h"
#include "lom_native_resident.h"
#include "lom_native_unit_decode.h"

#include "common/memory_map.h"
#include "main/cd_resources.h"
#include "main/cdrom.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Hash bytes with 32-bit FNV-1a.
 * @param bytes The bytes.
 * @param size How many.
 * @return The hash.
 */
static uint32_t fnv1a(const unsigned char* bytes, size_t size)
{
    uint32_t hash = 0x811C9DC5u;
    for (size_t i = 0; i < size; i++)
    {
        hash = (hash ^ bytes[i]) * 0x01000193u;
    }
    return hash;
}

/**
 * @brief Check each of a unit's globals encodes back to the bytes in the PS1's
 *        image.
 * @param unit The unit.
 * @param image The unit's PS1 image.
 * @return How many differ.
 */
static int check(const LomUnit* unit, const unsigned char* image)
{
    int differing = 0;
    size_t checked = 0;

    for (size_t i = 0; i < unit->global_count; i++)
    {
        const LomUnitGlobal* global = &unit->globals[i];
        if (global->encode == NULL)
        {
            continue;
        }
        unsigned char* bytes = calloc(1, global->size);
        global->encode(bytes);
        const unsigned char* original = image + (global->address - unit->vram);
        int same = memcmp(bytes, original, global->size) == 0;
        printf("  %-40s %08X %6u bytes %08X%s\n", global->name, global->address, global->size, fnv1a(bytes, global->size), same ? "" : "  DIFFERS");
        differing += !same;
        checked++;
        free(bytes);
    }

    printf("%s: %zu globals checked, %d differ\n", unit->name, checked, differing);
    return differing;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("usage: %s <extracted-disc-root>\n", argv[0]);
        return 2;
    }

    if (lom_native_cd_set_disc_root(argv[1]) != 0)
    {
        printf("Invalid extracted-disc root: %s\n", argv[1]);
        return 3;
    }
    cdrom_init();

    if (lom_native_load_resident(argv[1]) != 0)
    {
        return 4;
    }
    int differing = check(&lom_unit_slus, (const unsigned char*)MEMORY_MAP_RAM(lom_unit_slus.vram));

    unsigned char* window = (unsigned char*)OVERLAY_LOAD_BASE_ADDRESS;
    s32 size = cdrom_stream(CD_RES_TITLE_BIN, window);
    if (size != (s32)lom_unit_title.image_size)
    {
        printf("TITLE.BIN is %d bytes; this build expects %u.\n", size, lom_unit_title.image_size);
        return 5;
    }
    differing += check(&lom_unit_title, window);

    return differing == 0 ? 0 : 1;
}
