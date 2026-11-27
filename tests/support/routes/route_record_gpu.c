#include "route_record_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    short x, y, w, h;
} RouteRect;

enum
{
    PIXEL_BYTES = 2,
    AUDIO_CHANNELS = 2,
};

enum
{
    TAG_LOAD_IMAGE = 0xA0,
    TAG_CLEAR = 0x02,
    TAG_MOVE = 0x80,
    TAG_TEXPAGE = 0xE1,
    TAG_TEXTURE_WINDOW = 0xE2,
    TAG_AREA_START = 0xE3,
    TAG_AREA_END = 0xE4,
    TAG_OFFSET = 0xE5,
    TAG_MASK = 0xE6,
    TAG_READBACK = 0xC0,
};

enum
{
    READBACK_LIMIT = 8,
};

enum
{
    GP0_COMMAND_SHIFT = 24,
    GP0_RAW_TEXTURE = 0x01,
    GP0_TEXTURED = 0x04,
    GP0_QUAD = 0x08,
    GP0_POLYLINE = 0x08,
    GP0_GOURAUD = 0x10,
    GP0_FIRST_POLYGON = 0x20,
    GP0_FIRST_LINE = 0x40,
    GP0_FIRST_RECTANGLE = 0x60,
    GP0_FIRST_OTHER = 0x80,
    GP0_MAX_PRIMITIVE_WORDS = 64,
};

static const unsigned GP0_POLYLINE_END_MASK = 0xF000F000u;

static const unsigned GP0_POLYLINE_END = 0x50005000u;

static const unsigned GP0_COMMAND_BITS = 0xFF000000u;

static const unsigned GP0_COLOR_BITS = 0x00FFFFFFu;

static const unsigned GP0_TEXCOORD_BITS = 0x0000FFFFu;

static const unsigned GP0_TEXPAGE_BITS = 0xFFFF0000u;

static const char SOUND_DUMP_VARIABLE[] = "LOM_ROUTE_SOUND_DUMP";

static const char GPU_DUMP_VARIABLE[] = "LOM_ROUTE_GPU_DUMP";

static unsigned long long s_picture = ROUTE_FNV_OFFSET;

static unsigned long long s_screen = ROUTE_FNV_OFFSET;

static unsigned long long s_sound = ROUTE_FNV_OFFSET;

static long s_dump_frame = -1;

static char* s_dump_text;

static size_t s_dump_bytes;

static long s_dump_occurrence;

static long s_dump_seen;

static FILE* s_dump;

typedef struct
{
    const unsigned char* start;

    const unsigned char* end;
} Readback;

static Readback s_readbacks[READBACK_LIMIT];

static unsigned s_readback_count;

/**
 * @brief Mix a tagged GPU call and its numbers into the picture hash.
 * @param tag Which call it was.
 * @param a First number.
 * @param b Second number.
 * @param c Third number.
 * @param d Fourth number.
 */
static void mix_call(unsigned tag, int a, int b, int c, int d)
{
    int words[5] = {(int)tag, a, b, c, d};

    route_record_mix(&s_picture, words, sizeof(words));
    route_record_mix(&s_screen, words, sizeof(words));

    if (s_dump)
    {
        fprintf(s_dump, "call %u %d %d %d %d\n", tag, a, b, c, d);
    }
}

#ifndef _WIN32
FILE* open_memstream(char** text, size_t* bytes);
#endif

/**
 * @brief Start capturing the GPU commands of the frame, when a GPU dump was
 *        asked for.
 */
static void open_gpu_dump(void)
{
    if (s_dump_frame >= 0)
    {
#ifdef _WIN32
        s_dump = tmpfile();
#else
        s_dump = open_memstream(&s_dump_text, &s_dump_bytes);
#endif
    }
}

/**
 * @brief Stop capturing the frame's GPU commands, and save them as gpu-N.txt
 *        when this is the frame and occurrence asked for.
 * @param frame The frame's number.
 */
static void close_gpu_dump(unsigned frame)
{
    char path[ROUTE_RECORD_PATH_BYTES];
    FILE* file;

    if (!s_dump)
    {
        return;
    }

#ifdef _WIN32
    long size = fseek(s_dump, 0, SEEK_END) == 0 ? ftell(s_dump) : -1;

    if (size >= 0 && (s_dump_text = malloc((size_t)size + 1)) != NULL)
    {
        rewind(s_dump);
        s_dump_bytes = fread(s_dump_text, 1, (size_t)size, s_dump);
    }
#endif
    fclose(s_dump);
    s_dump = NULL;

    if ((long)frame == s_dump_frame && s_dump_seen++ == s_dump_occurrence)
    {
        snprintf(path, sizeof(path), "%s/gpu-%u.txt", route_record_directory(), frame);
        file = fopen(path, "w");

        if (file)
        {
            fwrite(s_dump_text, 1, s_dump_bytes, file);
            fclose(file);
        }
    }

    free(s_dump_text);
    s_dump_text = NULL;
    s_dump_bytes = 0;
}

void route_record_gpu_open(void)
{
    const char* frame = getenv(GPU_DUMP_VARIABLE);

    if (frame)
    {
        char* end;

        s_dump_frame = strtol(frame, &end, 0);
        s_dump_occurrence = *end == '@' ? strtol(end + 1, NULL, 0) : 0;
    }

    open_gpu_dump();
}

unsigned long long route_record_picture(void)
{
    return s_picture;
}

unsigned long long route_record_screen_picture(void)
{
    const unsigned long long screen = s_screen;
    s_screen = ROUTE_FNV_OFFSET;
    return screen == ROUTE_FNV_OFFSET ? 0 : screen;
}

unsigned long long route_record_sound(void)
{
    return s_sound;
}

void route_record_gpu_frame_start(void)
{
    s_picture = s_sound = s_screen = ROUTE_FNV_OFFSET;
    s_readback_count = 0;
}

void route_record_gpu_frame_end(unsigned frame)
{
    close_gpu_dump(frame);
    open_gpu_dump();
}

#ifdef _WIN64
typedef unsigned long long RouteWord;
#else
typedef unsigned long RouteWord;
#endif

extern int __real_Draw_PushPrim(RouteWord* packets, int max_len);

extern void __real_Draw_LoadImage(RouteRect* rect, RouteWord* pixels);

extern void __real_Draw_ClearImage(RouteRect* rect, unsigned char r, unsigned char g, unsigned char b);

extern void __real_Draw_MoveImage(RouteRect* rect, unsigned int x, unsigned int y);

extern void __real_Draw_StoreImage(RouteRect* rect, RouteWord* pixels);

extern void __real_Draw_SetTexpageMode(void* mode);

extern void __real_Draw_SetTextureWindow(unsigned int mask_x, unsigned int mask_y, unsigned int off_x, unsigned int off_y);

extern void __real_Draw_SetAreaStart(int x, int y);

extern void __real_Draw_SetAreaEnd(int x, int y);

extern void __real_Draw_SetOffset(int x, int y);

extern void __real_Draw_SetMask(int bit0, int bit1);

extern void __real_Psyz_SpuPullSamples(short* out, int num_frames);

/**
 * @brief The bits of each of a primitive's GP0 words that the GPU reads, for
 *        the picture fingerprint.
 * @param words The primitive's words; the first holds the command.
 * @param count How many words the primitive used.
 * @param read Receives each word's bits.
 */
static void gp0_read_bits(const unsigned* words, int count, unsigned* read)
{
    unsigned command = words[0] >> GP0_COMMAND_SHIFT;
    int raw = (command & GP0_TEXTURED) && (command & GP0_RAW_TEXTURE);
    int i = 1;

    for (int j = 0; j < count; j++)
    {
        read[j] = ~0u;
    }

    if (command < GP0_FIRST_POLYGON || command >= GP0_FIRST_OTHER)
    {
        return;
    }

    if (raw && command < GP0_FIRST_LINE)
    {
        read[0] = GP0_COMMAND_BITS;
    }

    if (command < GP0_FIRST_LINE)
    {
        int vertices = (command & GP0_QUAD) ? 4 : 3;
        int positions[4];
        int all_same = 1;

        for (int vertex = 0; vertex < vertices && i < count; vertex++)
        {
            if (vertex > 0 && (command & GP0_GOURAUD) && i < count)
            {
                read[i++] = GP0_COLOR_BITS;
            }

            positions[vertex] = i;
            all_same = all_same && (vertex == 0 || (i < count && words[i] == words[positions[0]]));
            i++;

            if ((command & GP0_TEXTURED) && i < count)
            {
                read[i++] = vertex >= 2 ? GP0_TEXCOORD_BITS : ~0u;
            }
        }

        if (all_same && i <= count)
        {
            for (int j = 0; j < i; j++)
            {
                read[j] = 0;
            }

            read[0] = GP0_COMMAND_BITS;

            for (int vertex = 0; vertex < vertices; vertex++)
            {
                read[positions[vertex]] = ~0u;
            }

            if (command & GP0_TEXTURED)
            {
                read[positions[1] + 1] = GP0_TEXPAGE_BITS;
            }
        }
    }
    else if (command < GP0_FIRST_RECTANGLE)
    {
        for (i = 2; i < count; i++)
        {
            if ((command & GP0_POLYLINE) && (words[i] & GP0_POLYLINE_END_MASK) == GP0_POLYLINE_END)
            {
                break;
            }

            if (command & GP0_GOURAUD)
            {
                read[i++] = GP0_COLOR_BITS;
            }
        }
    }
    else if (raw)
    {
        read[0] = GP0_COMMAND_BITS;
    }
}

/**
 * @brief Hash the GP0 words of a primitive the GPU reads, then draw it.
 * @param packets The GP0 words.
 * @param max_len Words available.
 * @return Words the primitive used.
 */
int __wrap_Draw_PushPrim(RouteWord* packets, int max_len)
{
    int used = __real_Draw_PushPrim(packets, max_len);
    int count = used < max_len ? used : max_len;
    int known = count < GP0_MAX_PRIMITIVE_WORDS ? count : GP0_MAX_PRIMITIVE_WORDS;
    unsigned words[GP0_MAX_PRIMITIVE_WORDS];
    unsigned read[GP0_MAX_PRIMITIVE_WORDS];

    for (int i = 0; i < known; i++)
    {
        words[i] = (unsigned)packets[i];
    }

    if (known > 0)
    {
        gp0_read_bits(words, known, read);
    }

    for (int i = 0; i < count; i++)
    {
        unsigned word = i < known ? words[i] & read[i] : (unsigned)packets[i];

        route_record_mix(&s_picture, &word, sizeof(word));
        route_record_mix(&s_screen, &word, sizeof(word));
    }

    if (s_dump)
    {
        fprintf(s_dump, "prim");

        for (int i = 0; i < count; i++)
        {
            fprintf(s_dump, " %08x", i < known ? words[i] & read[i] : (unsigned)packets[i]);
        }

        fprintf(s_dump, "\n");
    }

    return used;
}

/**
 * @brief Whether an upload's pixels overlap memory a VRAM read filled this
 *        frame.
 * @param pixels The upload's pixels.
 * @param bytes How many bytes it uploads.
 * @return True if any of them came from VRAM.
 */
static int read_back(const void* pixels, unsigned long bytes)
{
    const unsigned char* start = pixels;

    for (unsigned i = 0; i < s_readback_count; ++i)
    {
        if (start < s_readbacks[i].end && s_readbacks[i].start < start + bytes)
        {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Hash a texture upload, then do it.
 * @param rect VRAM rectangle.
 * @param pixels 16-bit pixels, packed.
 */
void __wrap_Draw_LoadImage(RouteRect* rect, RouteWord* pixels)
{
    unsigned long bytes = (unsigned long)rect->w * (unsigned long)rect->h * PIXEL_BYTES;

    mix_call(TAG_LOAD_IMAGE, rect->x, rect->y, rect->w, rect->h);

    if (read_back(pixels, bytes))
    {
        mix_call(TAG_READBACK, 0, 0, 0, 0);
    }
    else
    {
        route_record_mix(&s_picture, pixels, bytes);
        route_record_mix(&s_screen, pixels, bytes);

        if (s_dump)
        {
            fprintf(s_dump, "pixels %016llx\n", route_record_hash(pixels, bytes));
        }
    }

    __real_Draw_LoadImage(rect, pixels);
}

/**
 * @brief Read VRAM into memory, and remember that memory until the frame ends.
 * @param rect VRAM rectangle.
 * @param pixels Where the 16-bit pixels go, packed.
 */
void __wrap_Draw_StoreImage(RouteRect* rect, RouteWord* pixels)
{
    const unsigned char* start = (const unsigned char*)pixels;
    unsigned long bytes = (unsigned long)rect->w * (unsigned long)rect->h * PIXEL_BYTES;

    if (s_readback_count == READBACK_LIMIT)
    {
        memmove(s_readbacks, s_readbacks + 1, sizeof(s_readbacks) - sizeof(s_readbacks[0]));
        --s_readback_count;
    }

    s_readbacks[s_readback_count++] = (Readback){start, start + bytes};
    __real_Draw_StoreImage(rect, pixels);
}

/**
 * @brief Hash a clear, then do it.
 * @param rect VRAM rectangle.
 * @param r Red.
 * @param g Green.
 * @param b Blue.
 */
void __wrap_Draw_ClearImage(RouteRect* rect, unsigned char r, unsigned char g, unsigned char b)
{
    mix_call(TAG_CLEAR, rect->x, rect->y, rect->w, rect->h);
    mix_call(TAG_CLEAR, r, g, b, 0);
    __real_Draw_ClearImage(rect, r, g, b);
}

/**
 * @brief Hash a VRAM move, then do it.
 * @param rect Source rectangle.
 * @param x Destination x.
 * @param y Destination y.
 */
void __wrap_Draw_MoveImage(RouteRect* rect, unsigned int x, unsigned int y)
{
    mix_call(TAG_MOVE, rect->x, rect->y, rect->w, rect->h);
    mix_call(TAG_MOVE, (int)x, (int)y, 0, 0);
    __real_Draw_MoveImage(rect, x, y);
}

/**
 * @brief Hash a texture page change, then make it.
 * @param mode PSY-Z's texture page settings (one word of bit fields).
 */
void __wrap_Draw_SetTexpageMode(void* mode)
{
    unsigned word;

    memcpy(&word, mode, sizeof(word));
    mix_call(TAG_TEXPAGE, (int)word, 0, 0, 0);
    __real_Draw_SetTexpageMode(mode);
}

/**
 * @brief Hash a texture window change, then make it.
 * @param mask_x Mask x.
 * @param mask_y Mask y.
 * @param off_x Offset x.
 * @param off_y Offset y.
 */
void __wrap_Draw_SetTextureWindow(unsigned int mask_x, unsigned int mask_y, unsigned int off_x, unsigned int off_y)
{
    mix_call(TAG_TEXTURE_WINDOW, (int)mask_x, (int)mask_y, (int)off_x, (int)off_y);
    __real_Draw_SetTextureWindow(mask_x, mask_y, off_x, off_y);
}

/**
 * @brief Hash a drawing area start, then set it.
 * @param x Left.
 * @param y Top.
 */
void __wrap_Draw_SetAreaStart(int x, int y)
{
    mix_call(TAG_AREA_START, x, y, 0, 0);
    __real_Draw_SetAreaStart(x, y);
}

/**
 * @brief Hash a drawing area end, then set it.
 * @param x Right.
 * @param y Bottom.
 */
void __wrap_Draw_SetAreaEnd(int x, int y)
{
    mix_call(TAG_AREA_END, x, y, 0, 0);
    __real_Draw_SetAreaEnd(x, y);
}

/**
 * @brief Hash a drawing offset, then set it.
 * @param x Offset x.
 * @param y Offset y.
 */
void __wrap_Draw_SetOffset(int x, int y)
{
    mix_call(TAG_OFFSET, x, y, 0, 0);
    __real_Draw_SetOffset(x, y);
}

/**
 * @brief Hash a mask-bit setting, then set it.
 * @param bit0 Set the mask bit when drawing.
 * @param bit1 Check the mask bit when drawing.
 */
void __wrap_Draw_SetMask(int bit0, int bit1)
{
    mix_call(TAG_MASK, bit0, bit1, 0, 0);
    __real_Draw_SetMask(bit0, bit1);
}

/**
 * @brief Mix audio, then hash the samples.
 * @param out Stereo 16-bit output.
 * @param num_frames Sample frames to mix.
 */
void __wrap_Psyz_SpuPullSamples(short* out, int num_frames)
{
    static FILE* dump;
    static int dump_checked;

    __real_Psyz_SpuPullSamples(out, num_frames);
    route_record_mix(&s_sound, out, (unsigned long)num_frames * AUDIO_CHANNELS * sizeof(short));

    if (!dump_checked && route_record_directory()[0])
    {
        char path[ROUTE_RECORD_PATH_BYTES];
        dump_checked = 1;
        snprintf(path, sizeof(path), "%s/sound.raw", route_record_directory());
        dump = getenv(SOUND_DUMP_VARIABLE) ? fopen(path, "wb") : NULL;
    }

    if (dump)
    {
        fwrite(out, sizeof(short) * AUDIO_CHANNELS, (size_t)num_frames, dump);
        fflush(dump);
    }
}
