#include "lom_native_remaster.h"
#include "lom_native_cd.h"
#include "remaster_internal.h"
#include "../gpu/hd_textures.h"
#include "common/memory_map.h"

#define u_long LomPsyzULong
#include <psyz/gpu.h>
#undef u_long

#include "lom_native_psyz.h"
#include "lom_native_env.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PATH_SIZE 4096
#define PAGE_SIZE 1024
#define MAX_PAGES 32

#define COMMAND_SPRITE 0x14
#define COMMAND_QUAD 0x18
#define CELL_PS1 0x10000

static char s_root[PATH_SIZE];
static int s_found;

static int s_room_hd;

typedef struct
{
    int width, depth, hd_width, left;
} RoomSize;

static RoomSize s_rooms[16];
static int s_room_count;
static int s_swapped;
static size_t s_map_size;

#define RECORD_SLOTS (1u << 17)

typedef struct
{
    const void* record;
    unsigned cell;
} RecordCell;

static RecordCell* s_records;
static unsigned s_record_count;

/**
 * @brief Check whether a path is a folder.
 * @param path The path.
 * @return Nonzero when it is.
 */
static int is_directory(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

/**
 * @brief Use a folder as the Remaster's, if its data (windata/, or the folder
 *        itself) has the hdmap/ folder.
 * @param path The folder.
 * @return 1 when it's the Remaster, otherwise 0.
 */
static int use_root(const char* path)
{
    char probe[PATH_SIZE];
    const char* data[] = {"%s/windata", "%s"};

    for (unsigned i = 0; path && *path && i < sizeof(data) / sizeof(data[0]); ++i)
    {
        snprintf(s_root, sizeof(s_root), data[i], path);
        snprintf(probe, sizeof(probe), "%s/hdmap", s_root);

        if (is_directory(probe))
        {
            return 1;
        }
    }

    s_root[0] = 0;
    return 0;
}

/**
 * @brief Find the Remaster: LOM_REMASTER_ROOT, or Legend of Mana in one of
 *        Steam's usual library folders.
 * @return 1 when it's found, otherwise 0.
 */
static int find_root(void)
{
    const char* chosen = getenv(LOM_ENV_REMASTER_ROOT);

    if (chosen)
    {
        return use_root(chosen);
    }

    static const char* const steam_folders[] = {
#if defined(_WIN32)
        "C:/Program Files (x86)/Steam",
        "C:/Program Files/Steam",
#elif defined(__APPLE__)
        "~/Library/Application Support/Steam",
#else
        "~/.local/share/Steam",
        "~/.steam/steam",
        "~/.var/app/com.valvesoftware.Steam/.local/share/Steam",
#endif
    };
    const char* home = getenv("HOME");
    char path[PATH_SIZE];

    for (unsigned i = 0; i < sizeof(steam_folders) / sizeof(steam_folders[0]); ++i)
    {
        const char* folder = steam_folders[i];

        if (folder[0] == '~')
        {
            if (!home)
            {
                continue;
            }

            snprintf(path, sizeof(path), "%s%s/steamapps/common/Legend of Mana", home, folder + 1);
        }
        else
        {
            snprintf(path, sizeof(path), "%s/steamapps/common/Legend of Mana", folder);
        }

        if (use_root(path))
        {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Load and open one of the Remaster's PSB files.
 * @param folder Its folder in the Remaster's data.
 * @param name Its name.
 * @param psb Opened on it.
 * @return The file's bytes, which the caller frees, or NULL when it's missing
 *        or isn't a PSB.
 */
static unsigned char* open_psb(const char* folder, const char* name, LomPsb* psb)
{
    char path[PATH_SIZE];
    size_t size = 0;
    snprintf(path, sizeof(path), "%s/%s/%s", s_root, folder, name);
    unsigned char* data = lom_remaster_load(path, &size);

    if (data && lom_psb_open(psb, data, size) != 0)
    {
        fprintf(stderr, "[lom-native] The Remaster's %s/%s isn't a PSB file.\n", folder, name);
        free(data);
        return NULL;
    }

    return data;
}

/**
 * @brief Premultiply RGBA pixels by their alpha, as the renderer blends them.
 * @param pixels The pixels.
 * @param bytes Their size.
 */
static void premultiply(unsigned char* pixels, size_t bytes)
{
    for (size_t i = 0; i + 3 < bytes; i += 4)
    {
        const unsigned alpha = pixels[i + 3];

        if (alpha != 255)
        {
            pixels[i] = (unsigned char)((pixels[i] * alpha + 127) / 255);
            pixels[i + 1] = (unsigned char)((pixels[i + 1] * alpha + 127) / 255);
            pixels[i + 2] = (unsigned char)((pixels[i + 2] * alpha + 127) / 255);
        }
    }
}

static unsigned char* s_images_data;
static const unsigned char* s_page_pixels[MAX_PAGES];
static int s_page_count;

/**
 * @brief Forget the room's background pages.
 */
static void drop_pages(void)
{
    free(s_images_data);
    s_images_data = NULL;
    s_page_count = 0;
}

/**
 * @brief Read the label of one of the pages a room's map lists.
 * @param map The map.
 * @param list Its list of pages.
 * @param page Which page.
 * @return The label, or NULL when it hasn't one.
 */
static const char* page_label(const LomPsb* map, size_t list, size_t page)
{
    size_t entry, label;

    if (lom_psb_item(map, list, page, &entry) || lom_psb_member(map, entry, "label", &label))
    {
        return NULL;
    }

    return lom_psb_string(map, label);
}

/**
 * @brief Find the image with a label in a room's image file.
 * @param images The image file.
 * @param image_list Its list of images.
 * @param label The label, or NULL.
 * @return The image's entry, or 0 when there's none.
 */
static size_t find_image(const LomPsb* images, size_t image_list, const char* label)
{
    for (size_t i = 0; label && i < lom_psb_count(images, image_list); ++i)
    {
        size_t item, item_label, image;
        const char* text;

        if (!lom_psb_item(images, image_list, i, &item) && !lom_psb_member(images, item, "label", &item_label) && (text = lom_psb_string(images, item_label)) &&
            strcmp(text, label) == 0 && !lom_psb_path(images, item, "texture/0/image", &image))
        {
            return image;
        }
    }

    return 0;
}

/**
 * @brief Get an image's pixels, when it's a whole page of RGBA.
 * @param images The image file.
 * @param image The image's entry, or 0.
 * @return The pixels, or NULL.
 */
static const unsigned char* page_pixels(const LomPsb* images, size_t image)
{
    size_t pixel, bytes, type;
    const unsigned char* pixels;
    const char* kind;

    if (image && !lom_psb_member(images, image, "pixel", &pixel) && !lom_psb_chunk(images, pixel, &pixels, &bytes) &&
        bytes == (size_t)PAGE_SIZE * PAGE_SIZE * 4 && !lom_psb_member(images, image, "type", &type) && (kind = lom_psb_string(images, type)) &&
        strcmp(kind, "RGBA8") == 0)
    {
        return pixels;
    }

    return NULL;
}

/**
 * @brief Load a room's background pages from the Remaster, as RGBA pixels.
 * @param name The room's map name.
 * @param map The room's map PSB, which lists the pages.
 * @return 0, or -1 when they can't be loaded.
 */
static int load_pages(const char* name, const LomPsb* map)
{
    size_t list;
    drop_pages();

    if (lom_psb_member(map, map->root, "tex", &list))
    {
        return -1;
    }

    size_t pages = lom_psb_count(map, list);
    char file[64];
    LomPsb images;
    snprintf(file, sizeof(file), "%s_pack.psb.m", name);
    unsigned char* data = pages && pages <= MAX_PAGES ? open_psb("hdmapimg", file, &images) : NULL;

    if (!data)
    {
        return -1;
    }

    size_t image_list;
    int result = lom_psb_member(&images, images.root, "imageList", &image_list) ? -1 : 0;

    for (size_t page = 0; result == 0 && page < pages; ++page)
    {
        const unsigned char* pixels = page_pixels(&images, find_image(&images, image_list, page_label(map, list, page)));

        if (pixels)
        {
            s_page_pixels[page] = pixels;
        }
        else
        {
            result = -1;
        }
    }

    lom_psb_close(&images);

    if (result || lom_hd_textures_set((int)pages, PAGE_SIZE))
    {
        free(data);
        return -1;
    }

    s_images_data = data;
    s_page_count = (int)pages;
    return 0;
}

/**
 * @brief Recolor the Remaster's pixels the way the scene recolored the PS1's:
 *        by each pixel's brightest component, through the scene's 32-color
 *        table, blending between its steps.
 * @param pixels RGBA pixels.
 * @param bytes Their size.
 * @param table The scene's 32 15-bit colors.
 */
static void recolor(unsigned char* pixels, size_t bytes, const unsigned short* table)
{
    for (size_t i = 0; i + 3 < bytes; i += 4)
    {
        unsigned level = pixels[i] > pixels[i + 1] ? pixels[i] : pixels[i + 1];
        level = level > pixels[i + 2] ? level : pixels[i + 2];
        const unsigned scaled = level * 31;
        const unsigned low = scaled / 255, high = low < 31 ? low + 1 : 31, part = scaled % 255;

        for (unsigned c = 0; c < 3; ++c)
        {
            const unsigned shift = c == 0 ? 10 : c == 1 ? 5 : 0;
            const unsigned a = (table[low] >> shift) & 31, b = (table[high] >> shift) & 31;
            pixels[i + c] = (unsigned char)(((a * (255 - part) + b * part) / 255) * 255 / 31);
        }
    }
}

static void forget_room(void);

void lom_remaster_map_ready(const unsigned short* lookup)
{
    if (!s_swapped)
    {
        forget_room();
    }

    s_swapped = 0;

    if (!s_room_hd || !s_images_data)
    {
        drop_pages();
        return;
    }

    for (int page = 0; page < s_page_count; ++page)
    {
        unsigned char* pixels = (unsigned char*)s_page_pixels[page];
        const size_t bytes = (size_t)PAGE_SIZE * PAGE_SIZE * 4;

        if (lookup)
        {
            recolor(pixels, bytes, lookup);
        }

        premultiply(pixels, bytes);
        lom_hd_textures_upload(page, pixels);
    }

    drop_pages();
}

/**
 * @brief Forget the room's Remaster art and the tiles remembered for it.
 */
static void forget_room(void)
{
    s_room_hd = 0;
    s_swapped = 0;
    s_room_count = 0;
    s_record_count = 0;

    if (s_records)
    {
        memset(s_records, 0, RECORD_SLOTS * sizeof(*s_records));
    }
}

#define MAP_FILE_ADDRESS 0x80180000u
#define MAP_OBJECTS_WORD 0x20
#define MAP_OBJECT_WIDTH 0x30
#define MAP_OBJECT_DEPTH 0x32
#define MAX_MAP_OBJECTS 16

/**
 * @brief Read a word of the room's map file by its PS1 address.
 * @param map The map file.
 * @param size Its size.
 * @param address The PS1 address.
 * @return The word, or 0 when it's outside the file.
 */
static uint32_t map_word(const unsigned char* map, size_t size, uint32_t address)
{
    const uint32_t at = address - MAP_FILE_ADDRESS;
    return address >= MAP_FILE_ADDRESS && at + 4 <= size
               ? (uint32_t)map[at] | (uint32_t)map[at + 1] << 8 | (uint32_t)map[at + 2] << 16 | (uint32_t)map[at + 3] << 24
               : 0;
}

/**
 * @brief Find each map object's size word in the room's map file.
 * @param map The map file.
 * @param size Its size.
 * @param offsets Set to each size word's offset in the file.
 * @return How many objects, or -1 for a broken list.
 */
static int map_object_sizes(const unsigned char* map, size_t size, uint32_t offsets[MAX_MAP_OBJECTS])
{
    const uint32_t list = map_word(map, size, MAP_FILE_ADDRESS + MAP_OBJECTS_WORD);
    int count = 0;

    for (uint32_t object; list && count < MAX_MAP_OBJECTS && (object = map_word(map, size, list + (uint32_t)count * 4)) != 0; ++count)
    {
        offsets[count] = object - MAP_FILE_ADDRESS + MAP_OBJECT_WIDTH;

        if (offsets[count] + 4 > size)
        {
            return -1;
        }
    }

    return count;
}

#define REMASTER_LANGUAGE_BIT (1 << 1)

/**
 * @brief Use the Remaster's version of a map for the text language, when
 *        mapNameList.psb.m lists one.
 * @param name The map's name, replaced with the other version's.
 * @param size Bytes at @p name.
 */
static void language_version(char* name, size_t size)
{
    static int s_tried;
    static unsigned char* s_names_data;
    static LomPsb s_names;
    size_t languages, entry, mask, other;
    const char* other_name;

    if (!s_tried)
    {
        s_tried = 1;
        s_names_data = open_psb(".", "mapNameList.psb.m", &s_names);
    }

    if (s_names_data && !lom_psb_member(&s_names, s_names.root, "LangMap", &languages) && !lom_psb_member(&s_names, languages, name, &entry) &&
        !lom_psb_member(&s_names, entry, "mask", &mask) && (lom_psb_int(&s_names, mask) & REMASTER_LANGUAGE_BIT) &&
        !lom_psb_member(&s_names, entry, "name", &other) && (other_name = lom_psb_string(&s_names, other)) && strlen(other_name) < size)
    {
        memcpy(name, other_name, strlen(other_name) + 1);
    }
}

/**
 * @brief Find where the Remaster's art for one of a map's rooms starts, from
 *        its offset list.
 * @param map The map's PSB.
 * @param index The room.
 * @return Its left edge, in the PS1's pixels.
 */
static int room_left(const LomPsb* map, int index)
{
    char path[32];
    size_t pair, item;
    int64_t offset = 0;
    snprintf(path, sizeof(path), "offset/%d", index);

    if (lom_psb_path(map, map->root, path, &pair))
    {
        return 0;
    }

    if (lom_psb_kind(map, pair) == LOM_PSB_INT_ARRAY && lom_psb_count(map, pair) > 0)
    {
        offset = lom_psb_array_item(map, pair, 0);
    }
    else if (lom_psb_kind(map, pair) == LOM_PSB_LIST && !lom_psb_item(map, pair, 0, &item))
    {
        offset = lom_psb_int(map, item);
    }

    return (int)((offset * 2 + (offset < 0 ? -4 : 4)) / 9);
}

/**
 * @brief The name the Remaster gives a room's map: its file's name in lower
 *        case, in the version for the language chosen.
 * @param disc_name The map's file on the disc, like MAP/MAP012.MPD.
 * @param name Receives the name.
 * @param size Bytes at @p name.
 */
static void room_map_name(const char* disc_name, char* name, size_t size)
{
    const char* base = strrchr(disc_name, '/') + 1;
    size_t n = 0;

    for (; base[n] && base[n] != '.' && n + 1 < size; ++n)
    {
        name[n] = (char)tolower((unsigned char)base[n]);
    }

    name[n] = 0;
    language_version(name, size);
}

/**
 * @brief Swap a room's map for the Remaster's HD one, keeping the PS1's room
 *        sizes in it, and load the pages of art it draws with. When the
 *        Remaster's can't be used, the room keeps the PS1's.
 * @param data The PS1's map, replaced in place.
 * @param size Its size.
 * @param room Bytes the buffer has room for.
 * @param name The room's map name.
 * @return The map's size now.
 */
static int32_t use_hd_map(unsigned char* data, int32_t size, size_t room, const char* name)
{
    char file[64];
    snprintf(file, sizeof(file), "%s_pack.psb.m", name);
    LomPsb map;
    unsigned char* map_data = open_psb("hdmap", file, &map);
    size_t mpd;
    const unsigned char* bytes = NULL;
    size_t map_size = 0;
    size_t buffer = (size_t)((unsigned char*)CD_STREAM_STAGING_ADDRESS - (unsigned char*)LOAD_BUFFER_ADDRESS);

    if (map_data && !lom_psb_member(&map, map.root, "mpd", &mpd) && !lom_psb_chunk(&map, mpd, &bytes, &map_size) && map_size <= room && map_size <= buffer &&
        map_size >= 8 && memcmp(bytes, "SKmapDat", 8) == 0 && load_pages(name, &map) == 0)
    {
        uint32_t ps1_sizes[MAX_MAP_OBJECTS], hd_sizes[MAX_MAP_OBJECTS];
        unsigned char sizes[MAX_MAP_OBJECTS][4];
        const int ps1_objects = map_object_sizes(data, (size_t)size, ps1_sizes);

        for (int i = 0; i < ps1_objects; ++i)
        {
            memcpy(sizes[i], data + ps1_sizes[i], 4);
        }

        memcpy(data, bytes, map_size);

        if (ps1_objects > 0 && map_object_sizes(data, map_size, hd_sizes) == ps1_objects)
        {
            for (int i = 0; i < ps1_objects; ++i)
            {
                RoomSize* saved = &s_rooms[i];
                saved->width = (int16_t)(sizes[i][0] | sizes[i][1] << 8);
                saved->depth = (int16_t)(sizes[i][2] | sizes[i][3] << 8);
                saved->hd_width = (int16_t)(data[hd_sizes[i]] | data[hd_sizes[i] + 1] << 8);
                saved->left = room_left(&map, i);
                memcpy(data + hd_sizes[i], sizes[i], 4);
            }

            s_room_count = ps1_objects;
        }

        size = (int32_t)map_size;
        s_map_size = map_size;
        s_room_hd = 1;
        s_swapped = 1;
        fprintf(stderr, "[lom-native] The Remaster's %s.\n", name);
    }
    else
    {
        if (map_data)
        {
            fprintf(stderr, "[lom-native] The Remaster's %s couldn't be used; the room keeps the PS1's art.\n", name);
        }

        lom_hd_textures_release();
    }

    if (map_data)
    {
        lom_psb_close(&map);
        free(map_data);
    }

    return size;
}

/**
 * @brief Look at each file the game reads: rebuild a scene file's strings in
 *        the chosen language, or, for a room's map, set up the Remaster's art
 *        for it when HD backgrounds are on.
 * @param resource The file's resource index.
 * @param data The file's bytes.
 * @param size Their size.
 * @param room Bytes the buffer has room for.
 * @return The file's size, which a rebuilt scene file can change.
 */
static int32_t replace_map(int32_t resource, unsigned char* data, int32_t size, size_t room)
{
    const char* disc_name = lom_native_cd_resource_name(resource);
    size_t length = disc_name ? strlen(disc_name) : 0;

    if (disc_name && size > 0)
    {
        lom_remaster_effects_file(disc_name, data, (size_t)size);
    }

    if (length > 16 && (strncmp(disc_name, "ANA/MAPINFO/", 12) == 0 || strncmp(disc_name, "ANA/INFO_", 9) == 0) && strcmp(disc_name + length - 4, ".IMG") == 0)
    {
        return lom_remaster_text_scene(disc_name, data, size, room);
    }

    if (data != (unsigned char*)LOAD_BUFFER_ADDRESS || length < 9 || strncmp(disc_name, "MAP/MAP", 7) != 0 ||
        (strcmp(disc_name + length - 4, ".PRS") != 0 && strcmp(disc_name + length - 4, ".MPD") != 0))
    {
        return size;
    }

    forget_room();

    if (!s_records)
    {
        s_records = calloc(RECORD_SLOTS, sizeof(*s_records));
    }

    if (!lom_native_remaster_hd_backgrounds() || !lom_native_remaster_hd_supported() || !s_records)
    {
        if (lom_hd_textures_release)
        {
            lom_hd_textures_release();
        }

        return size;
    }

    char name[32];
    room_map_name(disc_name, name, sizeof(name));
    return use_hd_map(data, size, room, name);
}

/**
 * @brief Find where a tile record belongs in the table of tiles remembered.
 * @param record The record.
 * @return Its first slot to try.
 */
static unsigned slot_of(const void* record)
{
    uintptr_t key = (uintptr_t)record >> 2;
    return (unsigned)((key * 0x9E3779B1u) & (RECORD_SLOTS - 1));
}

/**
 * @brief Remember which of the Remaster's tiles a tile record stands for.
 * @param record The record.
 * @param cell The tile, or CELL_PS1 for the PS1's own.
 */
static void remember(const void* record, unsigned cell)
{
    for (unsigned slot = slot_of(record);; slot = (slot + 1) & (RECORD_SLOTS - 1))
    {
        if (s_records[slot].record == record)
        {
            s_records[slot].cell = cell;
            return;
        }

        if (!s_records[slot].record)
        {
            if (s_record_count >= RECORD_SLOTS / 4 * 3)
            {
                return;
            }

            s_records[slot].record = record;
            s_records[slot].cell = cell;
            ++s_record_count;
            return;
        }
    }
}

/**
 * @brief Find which of the Remaster's tiles a tile record stands for.
 * @param record The record.
 * @return The tile, or CELL_PS1 when it's the PS1's own.
 */
static unsigned cell_of(const void* record)
{
    for (unsigned slot = slot_of(record); s_records[slot].record; slot = (slot + 1) & (RECORD_SLOTS - 1))
    {
        if (s_records[slot].record == record)
        {
            return s_records[slot].cell;
        }
    }

    return CELL_PS1;
}

void lom_remaster_tile_record(const void* desc, const void* record)
{
    if (!s_room_hd || !s_records)
    {
        return;
    }

    const unsigned char* d = desc;
    const unsigned char* map = (const unsigned char*)LOAD_BUFFER_ADDRESS;
    remember(record, d >= map && d + 4 <= map + s_map_size ? ((((unsigned)d[0] & 1) << 4 | (d[1] & 0xF)) << 8) | d[2] : CELL_PS1);
}

void lom_remaster_tile_copied(const void* to, const void* from)
{
    if (s_room_hd && s_records)
    {
        remember(to, cell_of(from));
    }
}

void lom_remaster_tile(void* code, const void* record)
{
    if (!s_room_hd)
    {
        return;
    }

    const unsigned cell = s_records ? cell_of(record) : CELL_PS1;

    if (cell == CELL_PS1)
    {
        return;
    }

    uint32_t* words = code;
    unsigned op = words[0] >> 24;
    unsigned command = (op & 0xE0) == 0x20 ? COMMAND_QUAD : COMMAND_SPRITE;
    words[0] = (words[0] & 0xFFFFFF) | (uint32_t)(command | (op & 3)) << 24;
    words[2] = cell;
}

#define REMASTER_VIEW_WIDTH 427

int lom_remaster_layer_shift(int scale, int camera_x, int room_width, int room_depth)
{
    if (!s_room_hd || scale == 16)
    {
        return 0;
    }

    for (int i = 0; i < s_room_count; ++i)
    {
        if (s_rooms[i].width == room_width && s_rooms[i].depth == room_depth)
        {
            const int signed_scale = scale & 0x80 ? -(scale & 0x7F) : scale;
            return s_rooms[i].hd_width <= REMASTER_VIEW_WIDTH ? camera_x * (16 - signed_scale) / 16 : 0;
        }
    }

    return 0;
}

int lom_remaster_room_extent(int room_width, int room_depth, int* left, int* right)
{
    for (int i = 0; s_room_hd && i < s_room_count; ++i)
    {
        const RoomSize* room = &s_rooms[i];

        if (room->width == room_width && room->depth == room_depth)
        {
            if (room->left >= 0 && room->left + room->hd_width <= room_width)
            {
                return 0;
            }

            *left = room->left;
            *right = room->left + room->hd_width;
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Draw one of the Remaster's tiles in place of a PS1 one.
 * @param xy The four corners, as x and y pairs.
 * @param code The PS1 packet's code word: its color and flags.
 * @param cell The tile: page in the high byte, cell in the low.
 * @param tpage The texture page the PS1 used.
 */
static void draw_cell(const int xy[8], uint32_t code, uint32_t cell, int tpage)
{
    const unsigned flags = code >> 24;
    const unsigned rgb = flags & 1 ? 0x808080 : code & 0xFFFFFF;
    lom_hd_textures_draw(xy, (int)(cell >> 8 & 0xFF), (int)(cell & 0xFF), rgb, flags & 2, tpage);
}

/**
 * @brief Draw the Remaster tile command made from a 16-pixel sprite.
 * @param words The command's words.
 * @param available How many there are.
 * @param userdata Unused.
 * @return How many words it took, or 0 when there aren't enough.
 */
static int draw_sprite(const LomPsyzULong* words, int available, void* userdata)
{
    (void)userdata;

    if (available < 3)
    {
        return 0;
    }

    uint32_t xy = (uint32_t)words[1];
    int x = (int16_t)(xy & 0xFFFF), y = (int16_t)(xy >> 16);
    const int points[8] = {x, y, x + 16, y, x, y + 16, x + 16, y + 16};
    draw_cell(points, (uint32_t)words[0], (uint32_t)words[2], -1);
    return 3;
}

/**
 * @brief Draw the Remaster tile command made from a four-point textured
 *        polygon.
 * @param words The command's words.
 * @param available How many there are.
 * @param userdata Unused.
 * @return How many words it took, or 0 when there aren't enough.
 */
static int draw_quad(const LomPsyzULong* words, int available, void* userdata)
{
    (void)userdata;

    if (available < 9)
    {
        return 0;
    }

    int points[8];

    for (int i = 0; i < 4; ++i)
    {
        uint32_t xy = (uint32_t)words[1 + i * 2];
        points[i * 2] = (int16_t)(xy & 0xFFFF);
        points[i * 2 + 1] = (int16_t)(xy >> 16);
    }

    draw_cell(points, (uint32_t)words[0], (uint32_t)words[2], (int)((uint32_t)words[4] >> 16));
    return 9;
}

int lom_native_remaster_initialize(void)
{
#if defined(VERSION_JP)
    lom_native_remaster_set_text_supported(0);
#endif

    if (s_found || !find_root())
    {
        return s_found;
    }

    s_found = 1;
    lom_remaster_set_found(s_root);

    const char* hd = lom_native_env_value(LOM_ENV_REMASTER_HD);
    const char* music = lom_native_env_value(LOM_ENV_REMASTER_MUSIC);
    const char* ui = lom_native_env_value(LOM_ENV_REMASTER_UI);
    const char* text = lom_native_env_value(LOM_ENV_REMASTER_TEXT);

    if (text)
    {
        lom_native_remaster_set_text(atoi(text));
    }

    if (hd)
    {
        lom_native_remaster_set_hd_backgrounds(atoi(hd));
    }

    if (ui)
    {
        lom_native_remaster_set_hd_ui(atoi(ui));
    }

    if (music)
    {
        lom_native_remaster_set_music(atoi(music));
    }

    fprintf(stderr, "[lom-native] Using the Remaster's files in %s.\n", s_root);
    lom_native_cd_set_replacement(replace_map);

    for (unsigned bits = 0; bits < 4; ++bits)
    {
        Psyz_GpuRegisterCommandHandler(COMMAND_SPRITE | bits, draw_sprite, NULL);
        Psyz_GpuRegisterCommandHandler(COMMAND_QUAD | bits, draw_quad, NULL);
        lom_psyz_gpu_command_is_packet(COMMAND_SPRITE | bits, 1);
        lom_psyz_gpu_command_is_packet(COMMAND_QUAD | bits, 1);
    }

    lom_remaster_ui_initialize();
    return 1;
}

void lom_native_remaster_start_music(void)
{
    if (s_found)
    {
        lom_remaster_music_start();
        lom_remaster_effects_start();
    }
}
