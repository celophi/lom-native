#include "lom_native_remaster.h"
#include "remaster_internal.h"
#include "../gpu/hd_textures.h"

#define u_long LomPsyzULong
#include <psyz/gpu.h>
#undef u_long
#include "lom_native_psyz.h"
#include "lom_native_env.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMMAND_PICTURE 0x1C
#define PICTURE_NONE 0xFFFFFFFFu

#define LAYERS 4
#define LAYER_SIZE 2048
#define MARGIN 8
#define MAX_IMAGES 256
#define PLACEMENTS 1024
#define PATH_SIZE 4096
#define MAX_CHAIN 8

typedef struct
{
    char key[96];
    int layer;
    int x, y, w, h;
    unsigned stamp;
    int missing;
} Image;

typedef struct
{
    int shelf_y, shelf_h, cursor_x;
} Layer;

typedef struct
{
    int layer;
    int uv[8];
    int xy[8];
    int has_xy;
    int tpage;
    int next;
    int rgb;
    int semi;
} Placement;

static Image s_images[MAX_IMAGES];
static int s_image_count;
static Layer s_layers[LAYERS];
static unsigned s_stamp;
static int s_texture;
static Placement s_placements[PLACEMENTS];
static unsigned s_next_placement;

/**
 * @brief Check whether the renderer has the calls for the Remaster's pictures.
 * @return Nonzero when it does.
 */
static int supported(void)
{
    return lom_hd_images_set && lom_hd_images_upload && lom_hd_images_draw;
}

/**
 * @brief Multiply RGBA pixels' colors by their alpha, the way the GPU blends
 *        them.
 * @param pixels The pixels.
 * @param count How many.
 */
static void premultiply(unsigned char* pixels, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        unsigned char* p = pixels + i * 4;
        const unsigned a = p[3];

        if (a != 255)
        {
            p[0] = (unsigned char)(p[0] * a / 255);
            p[1] = (unsigned char)(p[1] * a / 255);
            p[2] = (unsigned char)(p[2] * a / 255);
        }
    }
}

/**
 * @brief Open one of the Remaster's picture files. The last one opened stays
 *        open for the next call.
 * @param file Its PSB file.
 * @return The file, or NULL when it can't be read.
 */
static const LomPsb* open_picture_file(const char* file)
{
    static char s_file[PATH_SIZE];
    static unsigned char* s_data;
    static LomPsb s_psb;

    if (!s_data || strcmp(s_file, file) != 0)
    {
        char path[PATH_SIZE];
        size_t size = 0;

        if (s_data)
        {
            lom_psb_close(&s_psb);
            free(s_data);
            s_data = NULL;
        }

        snprintf(path, sizeof(path), "%s/%s", lom_native_remaster_root(), file);
        snprintf(s_file, sizeof(s_file), "%s", file);
        s_data = lom_remaster_load(path, &size);

        if (s_data && lom_psb_open(&s_psb, s_data, size) != 0)
        {
            free(s_data);
            s_data = NULL;
        }

        if (!s_data)
        {
            return NULL;
        }
    }

    return &s_psb;
}

/**
 * @brief Read a number from a PSB object, if it has it.
 * @param psb The file.
 * @param object The object.
 * @param key The number's name.
 * @return The number, or 0 when the object doesn't have it.
 */
static int member_int(const LomPsb* psb, size_t object, const char* key)
{
    size_t value;

    return lom_psb_member(psb, object, key, &value) == 0 ? (int)lom_psb_int(psb, value) : 0;
}

/**
 * @brief Copy one of a picture's RGBA8 textures into its pixels, where it
 *        goes in the picture, cut to fit. Other kinds of texture are skipped.
 * @param psb The file.
 * @param texture The texture.
 * @param pixels The picture's pixels, with a margin around them.
 * @param width The picture's width.
 * @param height Its height.
 */
static void copy_texture(const LomPsb* psb, size_t texture, unsigned char* pixels, int width, int height)
{
    size_t image, value;
    const unsigned char* bytes;
    size_t byte_count;
    const char* type;

    if (lom_psb_member(psb, texture, "image", &image) != 0 || lom_psb_member(psb, image, "type", &value) != 0 || !(type = lom_psb_string(psb, value)) ||
        strcmp(type, "RGBA8") != 0 || lom_psb_member(psb, image, "pixel", &value) != 0 || lom_psb_chunk(psb, value, &bytes, &byte_count) != 0)
    {
        return;
    }

    const int left = member_int(psb, texture, "left"), top = member_int(psb, texture, "top");
    const int iw = member_int(psb, image, "width"), ih = member_int(psb, image, "height");
    int tw = member_int(psb, texture, "width"), th = member_int(psb, texture, "height");

    tw = tw < iw ? tw : iw;
    th = th < ih ? th : ih;
    tw = left + tw > width ? width - left : tw;
    th = top + th > height ? height - top : th;

    if (left < 0 || top < 0 || tw <= 0 || th <= 0 || byte_count < (size_t)iw * (size_t)ih * 4)
    {
        return;
    }

    const int slot_w = width + 2 * MARGIN;

    for (int row = 0; row < th; ++row)
    {
        memcpy(pixels + (((size_t)(MARGIN + top + row) * (size_t)slot_w) + (size_t)(MARGIN + left)) * 4, bytes + (size_t)row * (size_t)iw * 4, (size_t)tw * 4);
    }
}

/**
 * @brief Load one of the Remaster's pictures, or one labeled part of it, as
 *        RGBA pixels. The last file opened stays open for the next call.
 * @param file Its PSB file.
 * @param label The part's label, or NULL for the whole picture.
 * @param w Set to its width.
 * @param h Set to its height.
 * @return The pixels, which the caller frees, or NULL.
 */
static unsigned char* load_picture(const char* file, const char* label, int* w, int* h)
{
    const LomPsb* psb = open_picture_file(file);
    unsigned char* pixels = NULL;
    size_t list, item, textures;

    if (!psb || lom_psb_member(psb, psb->root, "imageList", &list) != 0)
    {
        return NULL;
    }

    for (size_t i = 0; i < lom_psb_count(psb, list) && !pixels; ++i)
    {
        size_t value;
        const char* name = NULL;

        if (lom_psb_item(psb, list, i, &item) != 0)
        {
            continue;
        }

        if (label && (lom_psb_member(psb, item, "label", &value) != 0 || !(name = lom_psb_string(psb, value)) || strcmp(name, label) != 0))
        {
            continue;
        }

        if (lom_psb_member(psb, item, "width", &value) != 0)
        {
            break;
        }

        const int width = (int)lom_psb_int(psb, value);

        if (lom_psb_member(psb, item, "height", &value) != 0)
        {
            break;
        }

        const int height = (int)lom_psb_int(psb, value);

        if (width <= 0 || height <= 0 || width + 2 * MARGIN > LAYER_SIZE || height + 2 * MARGIN > LAYER_SIZE ||
            lom_psb_member(psb, item, "texture", &textures) != 0)
        {
            break;
        }

        pixels = calloc((size_t)(width + 2 * MARGIN) * (size_t)(height + 2 * MARGIN), 4);

        for (size_t t = 0; pixels && t < lom_psb_count(psb, textures); ++t)
        {
            size_t texture;

            if (lom_psb_item(psb, textures, t, &texture) == 0)
            {
                copy_texture(psb, texture, pixels, width, height);
            }
        }

        *w = width;
        *h = height;
    }

    if (pixels)
    {
        premultiply(pixels, (size_t)(*w + 2 * MARGIN) * (size_t)(*h + 2 * MARGIN));
    }

    return pixels;
}

/**
 * @brief Empty one layer of the pictures' texture, forgetting the pictures in
 *        it.
 * @param layer The layer.
 */
static void empty_layer(int layer)
{
    s_layers[layer].shelf_y = s_layers[layer].shelf_h = s_layers[layer].cursor_x = 0;

    for (int i = 0; i < s_image_count; ++i)
    {
        if (s_images[i].layer == layer)
        {
            s_images[i].layer = -1;
        }
    }
}

/**
 * @brief Find room for a picture in a layer of the pictures' texture, filling
 *        it a row of shelves at a time.
 * @param layer The layer.
 * @param w The picture's width.
 * @param h Its height.
 * @param x Set to where it goes.
 * @param y Set to where it goes.
 * @return 0, or -1 when the layer is full.
 */
static int place(int layer, int w, int h, int* x, int* y)
{
    Layer* l = &s_layers[layer];

    if (l->cursor_x + w > LAYER_SIZE)
    {
        l->shelf_y += l->shelf_h;
        l->shelf_h = 0;
        l->cursor_x = 0;
    }

    if (l->shelf_y + h > LAYER_SIZE)
    {
        return -1;
    }

    *x = l->cursor_x;
    *y = l->shelf_y;
    l->cursor_x += w;
    l->shelf_h = h > l->shelf_h ? h : l->shelf_h;
    return 0;
}

/**
 * @brief Find the layer whose newest picture is oldest, to empty for a new one.
 * @return The layer.
 */
static int oldest_layer(void)
{
    unsigned newest[LAYERS] = {0};

    for (int i = 0; i < s_image_count; ++i)
    {
        if (s_images[i].layer >= 0 && s_images[i].stamp > newest[s_images[i].layer])
        {
            newest[s_images[i].layer] = s_images[i].stamp;
        }
    }

    int oldest = 0;

    for (int layer = 1; layer < LAYERS; ++layer)
    {
        if (newest[layer] < newest[oldest])
        {
            oldest = layer;
        }
    }

    empty_layer(oldest);
    return oldest;
}

/**
 * @brief Find a picture's entry among the ones asked for so far.
 * @param key Its file and label.
 * @return The entry, or NULL.
 */
static Image* find_image(const char* key)
{
    for (int i = 0; i < s_image_count; ++i)
    {
        if (strcmp(s_images[i].key, key) == 0)
        {
            return &s_images[i];
        }
    }

    return NULL;
}

/**
 * @brief Make an entry for a picture. When the list is full, the entries
 *        that are neither in the texture nor known to be missing go first.
 * @param key Its file and label.
 * @return The entry, or NULL when the list is still full.
 */
static Image* new_image(const char* key)
{
    if (s_image_count == MAX_IMAGES)
    {
        int kept = 0;

        for (int i = 0; i < s_image_count; ++i)
        {
            if (s_images[i].layer >= 0 || s_images[i].missing)
            {
                s_images[kept++] = s_images[i];
            }
        }

        s_image_count = kept;

        if (s_image_count == MAX_IMAGES)
        {
            return NULL;
        }
    }

    Image* image = &s_images[s_image_count++];
    memset(image, 0, sizeof(*image));
    snprintf(image->key, sizeof(image->key), "%s", key);
    image->layer = -1;
    return image;
}

/**
 * @brief Find room in the texture for a picture of some size: in the first
 *        layer with room, or else in the layer used least recently, emptied.
 * @param width Its width, with its margin, rounded up.
 * @param height Its height, the same way.
 * @param x Set to where it goes.
 * @param y The same.
 * @return The layer, or -1 when it doesn't fit anywhere.
 */
static int place_picture(int width, int height, int* x, int* y)
{
    for (int l = 0; l < LAYERS; ++l)
    {
        if (place(l, width, height, x, y) == 0)
        {
            return l;
        }
    }

    const int layer = oldest_layer();
    return place(layer, width, height, x, y) == 0 ? layer : -1;
}

/**
 * @brief Find one of the Remaster's pictures in the texture, loading and
 *        uploading it the first time it's asked for.
 * @param file Its PSB file.
 * @param label Its part's label, or NULL.
 * @return The picture, or NULL when it can't be drawn.
 */
static const Image* picture(const char* file, const char* label)
{
    char key[sizeof(s_images[0].key)];
    snprintf(key, sizeof(key), "%s#%s", file, label ? label : "");

    if (s_texture < 0 || !supported())
    {
        return NULL;
    }

    Image* image = find_image(key);

    if (image && image->missing)
    {
        return NULL;
    }

    if (image && image->layer >= 0)
    {
        image->stamp = ++s_stamp;
        return image;
    }

    if (!image && !(image = new_image(key)))
    {
        return NULL;
    }

    if (s_texture == 0)
    {
        s_texture = lom_hd_images_set(LAYERS, LAYER_SIZE) == 0 ? 1 : -1;

        if (s_texture < 0)
        {
            return NULL;
        }
    }

    int w = 0, h = 0;
    unsigned char* pixels = load_picture(file, label, &w, &h);

    if (!pixels)
    {
        image->missing = 1;
        return NULL;
    }

    const int slot_w = w + 2 * MARGIN, slot_h = h + 2 * MARGIN;
    int x = 0, y = 0;
    const int layer = place_picture((slot_w + 15) & ~15, (slot_h + 15) & ~15, &x, &y);

    if (layer < 0)
    {
        free(pixels);
        return NULL;
    }

    const int uploaded = lom_hd_images_upload(layer, x, y, slot_w, slot_h, pixels);
    free(pixels);

    if (uploaded != 0)
    {
        return NULL;
    }

    image->layer = layer;
    image->x = x + MARGIN;
    image->y = y + MARGIN;
    image->w = w;
    image->h = h;
    image->stamp = ++s_stamp;
    return image;
}

/**
 * @brief Turn the sprites the game built for a picture into one draw of the
 *        Remaster's picture: the first sprite becomes the command, the rest
 *        draw nothing.
 * @param code The first sprite's code word.
 * @param stride How far each next sprite's is, in bytes.
 * @param count How many sprites.
 * @param image The picture.
 * @param dst Where it goes on the screen.
 * @param uv Which part of the picture, as four corners.
 */
static void replace_sprites(void* code, int stride, int count, const Image* image, const int dst[4], const int uv[8])
{
    const unsigned index = s_next_placement++ % PLACEMENTS;
    Placement* placement = &s_placements[index];
    placement->layer = image->layer;
    placement->has_xy = 0;
    placement->tpage = -1;
    placement->next = -1;
    placement->rgb = -1;
    placement->semi = -1;

    for (int i = 0; i < 8; ++i)
    {
        placement->uv[i] = uv[i] + (i & 1 ? image->y : image->x);
    }

    for (int i = 0; i < count; ++i)
    {
        uint32_t* words = (uint32_t*)((unsigned char*)code + (size_t)i * (size_t)stride);
        const unsigned op = words[0] >> 24;
        words[0] = (words[0] & 0xFFFFFF) | (uint32_t)(COMMAND_PICTURE | (op & 3)) << 24;
        words[1] = (uint32_t)(dst[0] & 0xFFFF) | (uint32_t)(dst[1] & 0xFFFF) << 16;
        words[2] = i == 0 ? index : PICTURE_NONE;
        words[3] = (uint32_t)(dst[2] & 0xFFFF) | (uint32_t)(dst[3] & 0xFFFF) << 16;
    }
}

/**
 * @brief Draw the command replace_sprites made.
 * @param words The command's words.
 * @param available How many there are.
 * @param userdata Unused.
 * @return How many words it took, or 0 when there aren't enough.
 */
static int draw_picture(const LomPsyzULong* words, int available, void* userdata)
{
    (void)userdata;

    if (available < 4)
    {
        return 0;
    }

    uint32_t index = (uint32_t)words[2];
    const uint32_t code = (uint32_t)words[0], top_left = (uint32_t)words[1], bottom_right = (uint32_t)words[3];
    const int x0 = (int16_t)(top_left & 0xFFFF), y0 = (int16_t)(top_left >> 16);
    const int x1 = (int16_t)(bottom_right & 0xFFFF), y1 = (int16_t)(bottom_right >> 16);
    const int points[8] = {x0, y0, x1, y0, x0, y1, x1, y1};
    const unsigned flags = code >> 24;

    for (int drawn = 0; index < PLACEMENTS && drawn < MAX_CHAIN; ++drawn)
    {
        const Placement* placement = &s_placements[index];
        const unsigned rgb = placement->rgb >= 0 ? (unsigned)placement->rgb : flags & 1 ? 0x808080 : code & 0xFFFFFF;
        const int semi = placement->semi >= 0 ? placement->semi : (int)(flags & 2);
        lom_hd_images_draw(placement->has_xy ? placement->xy : points, placement->uv, placement->layer, rgb, semi, placement->tpage);
        index = placement->next < 0 ? PLACEMENTS : (uint32_t)placement->next;
    }

    return 4;
}

int lom_remaster_picture_ready(const char* file, const char* label, int* w, int* h)
{
    const Image* image = picture(file, label);

    if (image && w && h)
    {
        *w = image->w;
        *h = image->h;
    }

    return image != NULL;
}

int lom_remaster_picture_place(const char* file, const char* label, const int part[4], const int xy[8], int tpage, int after)
{
    const Image* image = picture(file, label);

    if (!image)
    {
        return -1;
    }

    const int px = part ? part[0] : 0, py = part ? part[1] : 0;
    const int pw = part ? part[2] : image->w, ph = part ? part[3] : image->h;

    if (px < 0 || py < 0 || pw <= 0 || ph <= 0 || px + pw > image->w || py + ph > image->h)
    {
        return -1;
    }

    const unsigned index = s_next_placement++ % PLACEMENTS;
    Placement* placement = &s_placements[index];
    const int u0 = image->x + px, v0 = image->y + py, u1 = u0 + pw, v1 = v0 + ph;
    const int uv[8] = {u0, v0, u1, v0, u0, v1, u1, v1};
    memcpy(placement->uv, uv, sizeof(uv));
    memcpy(placement->xy, xy, sizeof(placement->xy));
    placement->layer = image->layer;
    placement->has_xy = 1;
    placement->tpage = tpage;
    placement->next = -1;
    placement->rgb = -1;
    placement->semi = -1;

    if (after >= 0 && after < PLACEMENTS)
    {
        s_placements[after].next = (int)index;
    }

    return (int)index;
}

void lom_remaster_picture_style(int placement, int rgb, int semi)
{
    if (placement >= 0 && placement < PLACEMENTS)
    {
        s_placements[placement].rgb = rgb < 0 ? -1 : rgb & 0xFFFFFF;
        s_placements[placement].semi = semi < 0 ? -1 : semi != 0;
    }
}

void lom_remaster_picture_command(void* code, int words, int placement)
{
    uint32_t* w = code;
    const unsigned op = w[0] >> 24;
    w[0] = (w[0] & 0xFFFFFF) | (uint32_t)(COMMAND_PICTURE | (op & 3)) << 24;
    w[2] = placement >= 0 && placement < PLACEMENTS ? (uint32_t)placement : PICTURE_NONE;

    for (int i = 4; i < words; ++i)
    {
        w[i] = 0;
    }
}

void lom_remaster_ui_initialize(void)
{
    for (unsigned bits = 0; bits < 4; ++bits)
    {
        Psyz_GpuRegisterCommandHandler(COMMAND_PICTURE | bits, draw_picture, NULL);
        lom_psyz_gpu_command_is_packet(COMMAND_PICTURE | bits, 1);
    }
}

typedef struct
{
    const char* ps1;
    const char* name;
    int dst[4];
    int uv[8];
} EncyclopediaPicture;

static const EncyclopediaPicture s_encyclopedia[] = {
#include "remaster_picture_table.inc"
};

/**
 * @brief bsearch's comparison for the encyclopedia pictures, by the PS1 page's
 *        path.
 * @param key The path.
 * @param entry An EncyclopediaPicture.
 * @return Their strcmp order.
 */
static int compare_picture(const void* key, const void* entry)
{
    return strcmp((const char*)key, ((const EncyclopediaPicture*)entry)->ps1);
}

/**
 * @brief Find the Remaster's picture for an encyclopedia page, when HD pictures
 *        are on.
 * @param path The page's file on the disc.
 * @return Its entry, or NULL.
 */
static const EncyclopediaPicture* encyclopedia_entry(const char* path)
{
    if (!lom_native_remaster_found() || !lom_native_remaster_hd_ui())
    {
        return NULL;
    }

    const char* base = path ? strrchr(path, '/') : NULL;
    char name[16];
    base = base ? base + 1 : path;

    if (!base || strlen(base) < 6)
    {
        return NULL;
    }

    snprintf(name, sizeof(name), "%.6s", base);
    return bsearch(name, s_encyclopedia, sizeof(s_encyclopedia) / sizeof(s_encyclopedia[0]), sizeof(s_encyclopedia[0]), compare_picture);
}

void lom_remaster_encyclopedia_picture(void* code, int stride, int count, const char* path)
{
    const EncyclopediaPicture* entry = code && count > 0 ? encyclopedia_entry(path) : NULL;

    if (!entry)
    {
        return;
    }

    char file[64];
    snprintf(file, sizeof(file), "system/image/zukan/%s.psb.m", entry->name);
    const Image* image = picture(file, NULL);

    if (image)
    {
        replace_sprites(code, stride, count, image, entry->dst, entry->uv);
    }
}

int lom_remaster_encyclopedia_cover(const char* path, int top, int spacing, int height, int rows, int width, unsigned char* cover)
{
    const EncyclopediaPicture* entry = encyclopedia_entry(path);

    if (!entry || !cover || rows <= 0 || width <= 0)
    {
        return 0;
    }

    char file[64];
    int w = 0, h = 0;
    snprintf(file, sizeof(file), "system/image/zukan/%s.psb.m", entry->name);
    unsigned char* pixels = load_picture(file, NULL, &w, &h);

    if (!pixels)
    {
        return 0;
    }

    const int slot_w = w + 2 * MARGIN;
    const int x0 = entry->dst[0], y0 = entry->dst[1], x1 = entry->dst[2], y1 = entry->dst[3];
    const int* uv = entry->uv;
    memset(cover, 0, (size_t)rows * (size_t)width);

    for (int row = 0; row < rows; ++row)
    {
        for (int y = top + row * spacing; y < top + row * spacing + height; ++y)
        {
            if (y < y0 || y >= y1)
            {
                continue;
            }

            const float fy = ((float)y + 0.5f - (float)y0) / (float)(y1 - y0);

            for (int x = x0 > 0 ? x0 : 0; x < x1 && x < width; ++x)
            {
                const float fx = ((float)x + 0.5f - (float)x0) / (float)(x1 - x0);
                const float u = (1 - fx) * (1 - fy) * (float)uv[0] + fx * (1 - fy) * (float)uv[2] + (1 - fx) * fy * (float)uv[4] + fx * fy * (float)uv[6];
                const float v = (1 - fx) * (1 - fy) * (float)uv[1] + fx * (1 - fy) * (float)uv[3] + (1 - fx) * fy * (float)uv[5] + fx * fy * (float)uv[7];
                const int pu = (int)u, pv = (int)v;

                if (pu >= 0 && pu < w && pv >= 0 && pv < h && pixels[((size_t)(pv + MARGIN) * (size_t)slot_w + (size_t)(pu + MARGIN)) * 4 + 3] > 16 &&
                    cover[row * width + x] < 255)
                {
                    ++cover[row * width + x];
                }
            }
        }
    }

    free(pixels);
    return 1;
}

#define VRAM_WIDTH 1024
#define VRAM_HEIGHT 512
#define FACE_SIZE 48

typedef struct
{
    uint64_t hash;
    const char* file;
    const char* name;
    int mirrored;
} Face;

static const Face s_faces[] = {
#include "remaster_face_table.inc"
};

static uint16_t* s_vram;

/**
 * @brief PSY-Z's hook after the game uploads an image to video memory
 *        (gpu-texture-replacement.patch): keep a copy of it, to recognize faces
 *        when they're drawn.
 * @param x Where it went.
 * @param y Where it went.
 * @param w Its width.
 * @param h Its height.
 * @param data Its pixels.
 */
void lom_gpu_image_loaded(int x, int y, int w, int h, const void* data)
{
    if (!lom_native_remaster_found() || w <= 0 || h <= 0 || !data)
    {
        return;
    }

    if (!s_vram && !(s_vram = calloc((size_t)VRAM_WIDTH * VRAM_HEIGHT, sizeof(*s_vram))))
    {
        return;
    }

    const uint16_t* source = data;

    for (int row = 0; row < h; ++row)
    {
        for (int column = 0; column < w; ++column)
        {
            s_vram[((y + row) & (VRAM_HEIGHT - 1)) * VRAM_WIDTH + ((x + column) & (VRAM_WIDTH - 1))] = source[(size_t)row * (size_t)w + (size_t)column];
        }
    }
}

int lom_remaster_vram_bounds(int page_x, int page_y, int u, int v, int w, int h, int bounds[4])
{
    int left = w, top = h, right = -1, bottom = -1;

    if (!s_vram || w <= 0 || h <= 0)
    {
        return -1;
    }

    for (int row = 0; row < h; ++row)
    {
        for (int column = 0; column < w; ++column)
        {
            const int texel = u + column;
            const uint16_t word = s_vram[((page_y + v + row) & (VRAM_HEIGHT - 1)) * VRAM_WIDTH + ((page_x + texel / 4) & (VRAM_WIDTH - 1))];

            if ((word >> ((texel & 3) * 4)) & 0xF)
            {
                left = column < left ? column : left;
                right = column > right ? column : right;
                top = row < top ? row : top;
                bottom = row > bottom ? row : bottom;
            }
        }
    }

    if (right < 0)
    {
        return -1;
    }

    bounds[0] = left;
    bounds[1] = top;
    bounds[2] = right + 1 - left;
    bounds[3] = bottom + 1 - top;
    return 0;
}

/**
 * @brief PSY-Z's hook after the game moves part of video memory: move the copy
 *        the same way.
 * @param x Where it was.
 * @param y Where it was.
 * @param w Its width.
 * @param h Its height.
 * @param to_x Where it went.
 * @param to_y Where it went.
 */
void lom_gpu_image_moved(int x, int y, int w, int h, int to_x, int to_y)
{
    if (!s_vram || w <= 0 || h <= 0)
    {
        return;
    }

    uint16_t* copy = malloc((size_t)w * (size_t)h * sizeof(*copy));

    if (!copy)
    {
        return;
    }

    for (int row = 0; row < h; ++row)
    {
        for (int column = 0; column < w; ++column)
        {
            copy[row * w + column] = s_vram[((y + row) & (VRAM_HEIGHT - 1)) * VRAM_WIDTH + ((x + column) & (VRAM_WIDTH - 1))];
        }
    }

    lom_gpu_image_loaded(to_x, to_y, w, h, copy);
    free(copy);
}

/**
 * @brief Hash bytes into a 64-bit FNV-1a hash.
 * @param hash The hash so far.
 * @param data The bytes.
 * @param size How many.
 * @return The new hash.
 */
static uint64_t fnv(uint64_t hash, const void* data, size_t size)
{
    const unsigned char* bytes = data;

    for (size_t i = 0; i < size; ++i)
    {
        hash = (hash ^ bytes[i]) * UINT64_C(0x100000001b3);
    }

    return hash;
}

/**
 * @brief bsearch's comparison for the known faces, by their pixels' hash.
 * @param key The hash.
 * @param entry A Face.
 * @return Their order.
 */
static int compare_face(const void* key, const void* entry)
{
    const uint64_t a = *(const uint64_t*)key, b = ((const Face*)entry)->hash;
    return a < b ? -1 : a > b;
}

typedef struct
{
    int xy[8];
    int uv[8];
    int clut;
    int page;
    int used;
} FacePrim;

/**
 * @brief Read a primitive that could be a 48 by 48 face: a textured sprite of
 *        that size, or a textured quad.
 * @param words The primitive's words.
 * @param available How many there are.
 * @param texpage The texture page in use, for a sprite.
 * @param prim Receives its corners, texture coordinates, CLUT, page and size.
 * @return 1 when it could be a face, otherwise 0.
 */
static int read_face_prim(const unsigned int* words, int available, unsigned int texpage, FacePrim* prim)
{
    const unsigned op = words[0] >> 24;

    prim->page = (int)texpage;

    if ((op & 0xFC) == 0x64)
    {
        const int x = (int16_t)(words[1] & 0xFFFF), y = (int16_t)(words[1] >> 16);

        if ((words[3] & 0xFFFF) != FACE_SIZE || (words[3] >> 16) != FACE_SIZE)
        {
            return 0;
        }

        const int u = (int)(words[2] & 0xFF), v = (int)(words[2] >> 8 & 0xFF);
        const int points[8] = {x, y, x + FACE_SIZE, y, x, y + FACE_SIZE, x + FACE_SIZE, y + FACE_SIZE};
        const int texels[8] = {u, v, u + FACE_SIZE, v, u, v + FACE_SIZE, u + FACE_SIZE, v + FACE_SIZE};
        memcpy(prim->xy, points, sizeof(prim->xy));
        memcpy(prim->uv, texels, sizeof(prim->uv));
        prim->clut = (int)(words[2] >> 16);
        prim->used = 4;
        return 1;
    }

    if (available < 9)
    {
        return 0;
    }

    for (int i = 0; i < 4; ++i)
    {
        prim->xy[i * 2] = (int16_t)(words[1 + i * 2] & 0xFFFF);
        prim->xy[i * 2 + 1] = (int16_t)(words[1 + i * 2] >> 16);
        prim->uv[i * 2] = (int)(words[2 + i * 2] & 0xFF);
        prim->uv[i * 2 + 1] = (int)(words[2 + i * 2] >> 8 & 0xFF);
    }

    prim->clut = (int)(words[2] >> 16);
    prim->page = (int)(words[4] >> 16 & 0x1FF);
    prim->used = 9;
    return 1;
}

/**
 * @brief Hash a face's pixels and its palette, as they are in VRAM, to look
 *        it up in the faces the Remaster repainted.
 * @param vram_x Where its pixels start, in VRAM words.
 * @param vram_y The row they start on.
 * @param clut_x Where its palette is, in VRAM words.
 * @param clut_y The palette's row.
 * @return The hash.
 */
static uint64_t face_hash(int vram_x, int vram_y, int clut_x, int clut_y)
{
    uint64_t hash = UINT64_C(0xcbf29ce484222325);

    for (int row = 0; row < FACE_SIZE; ++row)
    {
        hash = fnv(hash, &s_vram[(vram_y + row) * VRAM_WIDTH + vram_x], FACE_SIZE / 4 * sizeof(*s_vram));
    }

    return fnv(hash, &s_vram[clut_y * VRAM_WIDTH + clut_x], 16 * sizeof(*s_vram));
}

/**
 * @brief With LOM_REMASTER_UI_LOG on, log each face the Remaster has no
 *        repainted picture for, once.
 * @param hash The face's hash.
 * @param vram_x Where it is in VRAM.
 * @param vram_y Its row.
 */
static void log_missing_face(uint64_t hash, int vram_x, int vram_y)
{
    static int s_log = -1;
    static uint64_t s_logged[64];
    static unsigned s_logged_count;

    if (s_log < 0)
    {
        s_log = lom_native_env_on(LOM_ENV_REMASTER_UI_LOG);
    }

    if (!s_log)
    {
        return;
    }

    for (unsigned i = 0; i < s_logged_count; ++i)
    {
        if (s_logged[i] == hash)
        {
            return;
        }
    }

    if (s_logged_count < sizeof(s_logged) / sizeof(s_logged[0]))
    {
        s_logged[s_logged_count++] = hash;
        fprintf(stderr, "[lom-native] No HD face for the 48x48 picture %016llx at %d,%d.\n", (unsigned long long)hash, vram_x, vram_y);
    }
}

/**
 * @brief PSY-Z's hook before it draws a primitive: when it draws a face the
 *        Remaster has repainted, draw that instead.
 * @param words The primitive's words.
 * @param available How many there are.
 * @param texpage The texture page in use.
 * @return How many of the primitive's words it took when it drew the
 *         Remaster's face, so PSY-Z skips them, or 0 to let PSY-Z draw it.
 */
int lom_gpu_prim_filter(const unsigned int* words, int available, unsigned int texpage)
{
    FacePrim prim;

    if (!s_vram || !lom_native_remaster_hd_ui() || available < 4 || !read_face_prim(words, available, texpage, &prim))
    {
        return 0;
    }

    int u0 = 255, v0 = 255, u1 = 0, v1 = 0;

    for (int i = 0; i < 4; ++i)
    {
        u0 = prim.uv[i * 2] < u0 ? prim.uv[i * 2] : u0;
        u1 = prim.uv[i * 2] > u1 ? prim.uv[i * 2] : u1;
        v0 = prim.uv[i * 2 + 1] < v0 ? prim.uv[i * 2 + 1] : v0;
        v1 = prim.uv[i * 2 + 1] > v1 ? prim.uv[i * 2 + 1] : v1;
    }

    const int span_u = u1 - u0, span_v = v1 - v0;

    if ((prim.page >> 7 & 3) != 0 || (u0 & 3) != 0 || (span_u != FACE_SIZE && span_u != FACE_SIZE - 1) || (span_v != FACE_SIZE && span_v != FACE_SIZE - 1))
    {
        return 0;
    }

    const int vram_x = (prim.page & 0xF) * 64 + u0 / 4, vram_y = (prim.page >> 4 & 1) * 256 + v0;
    const int clut_x = (prim.clut & 0x3F) * 16, clut_y = prim.clut >> 6 & 0x1FF;

    if (vram_x + FACE_SIZE / 4 > VRAM_WIDTH || vram_y + FACE_SIZE > VRAM_HEIGHT || clut_x + 16 > VRAM_WIDTH)
    {
        return 0;
    }

    const uint64_t hash = face_hash(vram_x, vram_y, clut_x, clut_y);
    const Face* face = bsearch(&hash, s_faces, sizeof(s_faces) / sizeof(s_faces[0]), sizeof(s_faces[0]), compare_face);
    const Image* image = face ? picture(face->file, face->name) : NULL;

    if (!image)
    {
        log_missing_face(hash, vram_x, vram_y);
        return 0;
    }

    const int span = span_u == FACE_SIZE ? FACE_SIZE : FACE_SIZE - 1;

    for (int i = 0; i < 4; ++i)
    {
        int across = prim.uv[i * 2] - u0, down = prim.uv[i * 2 + 1] - v0;
        across = face->mirrored ? span - across : across;
        prim.uv[i * 2] = image->x + across * image->w / span;
        prim.uv[i * 2 + 1] = image->y + down * image->h / (span_v == FACE_SIZE ? FACE_SIZE : FACE_SIZE - 1);
    }

    const unsigned flags = words[0] >> 24;
    lom_hd_images_draw(prim.xy, prim.uv, image->layer, flags & 1 ? 0x808080 : words[0] & 0xFFFFFF, flags & 2, prim.page);
    return prim.used;
}
