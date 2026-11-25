#include "lom_native_resident.h"
#include "lom_native_unit_decode.h"
#include "lom_native_cd.h"

#include "common/memory_map.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PSX_EXE_HEADER_SIZE 0x800u
#define PSX_EXE_TEXT_ADDRESS 0x18u
#define PSX_EXE_TEXT_SIZE 0x1Cu

static int s_loaded;

/**
 * @brief Read a little-endian 32-bit word.
 * @param bytes Its first byte.
 * @return The word.
 */
static uint32_t read_u32(const unsigned char* bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

int lom_native_load_resident(const char* disc_root)
{
    unsigned char header[PSX_EXE_HEADER_SIZE];
    char path[4096];

    if (s_loaded)
    {
        return 0;
    }

    if (snprintf(path, sizeof(path), "%s/" LOM_NATIVE_DISC_EXECUTABLE, disc_root) >= (int)sizeof(path))
    {
        return -1;
    }

    FILE* file = fopen(path, "rb");

    if (file == NULL)
    {
        fprintf(stderr, "[lom-native] Unable to open resident executable: %s\n", path);
        return -1;
    }

    if (fread(header, 1, sizeof(header), file) != sizeof(header) || memcmp(header, "PS-X EXE", 8) != 0)
    {
        fclose(file);
        fprintf(stderr, "[lom-native] Invalid PS-X EXE header in %s\n", path);
        return -1;
    }

    uint32_t address = read_u32(&header[PSX_EXE_TEXT_ADDRESS]);
    uint32_t size = read_u32(&header[PSX_EXE_TEXT_SIZE]);

    if (address != lom_unit_slus.vram || size < lom_unit_slus.image_size)
    {
        fclose(file);
        fprintf(stderr, "[lom-native] %s loads at 0x%08X, %u bytes; this build expects 0x%08X, %u bytes.\n", path, address, size, lom_unit_slus.vram,
                lom_unit_slus.image_size);
        return -1;
    }

    unsigned char* window = (unsigned char*)MEMORY_MAP_RAM(address);

    if (fread(window, 1, lom_unit_slus.image_size, file) != lom_unit_slus.image_size)
    {
        fclose(file);
        fprintf(stderr, "[lom-native] Failed to read %s.\n", path);
        return -1;
    }

    fclose(file);
    lom_unit_load(&lom_unit_slus, window, window);
    s_loaded = 1;
    return 0;
}
