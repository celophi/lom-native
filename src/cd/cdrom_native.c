#include "main/cdrom.h"
#include "main/cdrom_decompress.h"
#include "main/cd_resources.h"

#include "lom_native_cd.h"
#include "lom_native_boundary.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_unit_decode.h"
#include "lom_native_env.h"

#define u_long LomPsyzULong
#include <libcd.h>
#undef u_long

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LOM_NATIVE_CD_PATH_MAX 4096
#define CD_RESOURCE_INDEX_DEFAULT 0xFFFF
#define CALLBACK_READS 15

typedef struct LomNativeCdResourcePath
{
    int resource_index;

    const char* relative_path;

    s32 data_size;
} LomNativeCdResourcePath;

static const LomNativeCdResourcePath g_resource_paths[] = {
#if defined(VERSION_JP)
#include "disc_resource_paths_jp.inc"
#else
#include "disc_resource_paths.inc"
#endif
};

static char g_disc_root[LOM_NATIVE_CD_PATH_MAX] = "disc";

static int g_disc_root_explicit;

static LomNativeCdError g_last_error;

typedef struct NativeCallbackRead
{
    FILE* file;

    CdCommandCallback callback;

    u8* destination;

    s32 resource, size, delivered;

    uint64_t ticket;

    u8 command;
} NativeCallbackRead;

static NativeCallbackRead g_callback_reads[CALLBACK_READS];

static unsigned g_callback_head, g_callback_count, g_callback_generation;

static unsigned g_callback_reset_epoch;

static int g_callback_pumping;

static uint64_t g_callback_next_ticket;

/**
 * @brief Drop every queued callback read, closing their files, and start a new
 *        reset epoch so their owners know.
 */
static void cancel_callback_reads(void)
{
    while (g_callback_count)
    {
        if (g_callback_reads[g_callback_head].file)
        {
            fclose(g_callback_reads[g_callback_head].file);
        }

        memset(&g_callback_reads[g_callback_head], 0, sizeof(NativeCallbackRead));
        g_callback_head = (g_callback_head + 1) % CALLBACK_READS;
        --g_callback_count;
    }

    ++g_callback_generation;
    ++g_callback_reset_epoch;
}

unsigned lom_native_cd_reset_epoch(void)
{
    return g_callback_reset_epoch;
}

int lom_native_cd_callback_pending(int resource, LomNativeCdCallback callback)
{
    for (unsigned i = 0; i < g_callback_count; ++i)
    {
        NativeCallbackRead* read = &g_callback_reads[(g_callback_head + i) % CALLBACK_READS];

        if (read->resource == (u16)resource && read->callback == callback)
        {
            return 1;
        }
    }

    return 0;
}

unsigned lom_native_cd_cancel_callback(int resource, LomNativeCdCallback callback)
{
    NativeCallbackRead retained[CALLBACK_READS];
    unsigned keep = 0, removed = 0;

    for (unsigned i = 0; i < g_callback_count; ++i)
    {
        NativeCallbackRead* read = &g_callback_reads[(g_callback_head + i) % CALLBACK_READS];

        if (read->resource == (u16)resource && read->callback == callback)
        {
            if (read->file)
            {
                fclose(read->file);
            }

            ++removed;
        }
        else
        {
            retained[keep++] = *read;
        }
    }

    if (removed)
    {
        memset(g_callback_reads, 0, sizeof(g_callback_reads));
        memcpy(g_callback_reads, retained, keep * sizeof(*retained));
        g_callback_head = 0;
        g_callback_count = keep;
        ++g_callback_generation;
    }

    return removed;
}

/**
 * @brief Check that a read fits where it's going: in the PS1's RAM, in a buffer
 *        the port gave a decoded unit, or at a PS1 address.
 * @param destination Where it's going.
 * @param bytes How many bytes.
 * @return Nonzero when it fits.
 */
static int destination_fits(void* destination, size_t bytes)
{
    uintptr_t address = (uintptr_t)destination;

    if (!address)
    {
        return 0;
    }

    u32 narrow = (u32)address;

    if (address == (uintptr_t)(intptr_t)(s32)narrow)
    {
        address = narrow;
    }

    {
        uintptr_t ram_start = (uintptr_t)lom_native_ram;
        uintptr_t ram_end = ram_start + LOM_PS1_RAM_SIZE * MEMORY_MAP_SCALE;

        if ((uintptr_t)destination >= ram_start && (uintptr_t)destination <= ram_end)
        {
            return bytes <= ram_end - (uintptr_t)destination;
        }
    }

    {
        size_t left = lom_unit_host_bytes_left(destination);

        if (left != 0)
        {
            return bytes <= left;
        }
    }

    if (address >= LOM_PS1_RAM_BASE && address <= LOM_PS1_RAM_BASE + LOM_PS1_RAM_SIZE)
    {
        return bytes <= LOM_PS1_RAM_BASE + LOM_PS1_RAM_SIZE - address;
    }

    if (address >= LOM_PS1_RAM_ALIAS_BASE && address <= LOM_PS1_RAM_ALIAS_BASE + LOM_PS1_RAM_SIZE)
    {
        return bytes <= LOM_PS1_RAM_ALIAS_BASE + LOM_PS1_RAM_SIZE - address;
    }

    return bytes <= UINTPTR_MAX - address;
}

/**
 * @brief Run the queued callback reads to the end before a read that doesn't
 *        wait in the queue.
 * @return 0, or -1 with the last error set when one fails or stops making
 *        progress.
 */
static int drain_callback_reads(void)
{
    if (!g_callback_count)
    {
        return 0;
    }

    while (g_callback_count)
    {
        unsigned head = g_callback_head, count = g_callback_count;
        s32 delivered = g_callback_reads[head].delivered;
        g_last_error = LOM_NATIVE_CD_ERROR_NONE;
        cdrom_process_state();

        if (g_last_error != LOM_NATIVE_CD_ERROR_NONE)
        {
            return -1;
        }

        if (g_callback_count && head == g_callback_head && count == g_callback_count && delivered == g_callback_reads[head].delivered)
        {
            g_last_error = LOM_NATIVE_CD_ERROR_CALLBACK_BLOCKED;
            return -1;
        }
    }

    return g_last_error == LOM_NATIVE_CD_ERROR_NONE ? 0 : -1;
}

/**
 * @brief Find a resource's file on the disc.
 * @param resource_index The resource.
 * @return Its path from the disc's root, or NULL when it has none.
 */
static const char* find_resource_relative_path(int resource_index)
{
    size_t i;

    for (i = 0; i < sizeof(g_resource_paths) / sizeof(g_resource_paths[0]); i++)
    {
        if (g_resource_paths[i].resource_index == resource_index)
        {
            return g_resource_paths[i].relative_path;
        }
    }

    return NULL;
}

/**
 * @brief Measure a file.
 * @param path The file.
 * @return Its size, or -1 when it can't be read or is over 2 GB.
 */
static s32 get_file_size(const char* path)
{
    struct stat st;

    if (stat(path, &st) != 0 || st.st_size < 0 || st.st_size > 0x7FFFFFFF)
    {
        return -1;
    }

    return (s32)st.st_size;
}

/**
 * @brief Read a whole resource's file into memory, and set the last error.
 * @param resource_index The resource.
 * @param size_out Set to its size.
 * @return Its bytes, for the caller to free, or NULL.
 */
static u8* load_resource_file(int resource_index, s32* size_out)
{
    char path[LOM_NATIVE_CD_PATH_MAX];
    FILE* file;
    u8* data;
    s32 size;
    size_t bytes_read;

    if (size_out == NULL)
    {
        g_last_error = LOM_NATIVE_CD_ERROR_IO;
        return NULL;
    }

    if (lom_native_cd_resolve_resource_path(resource_index, path, sizeof(path)) != 0)
    {
        return NULL;
    }

    size = get_file_size(path);

    if (size <= 0)
    {
        g_last_error = (errno == ENOENT) ? LOM_NATIVE_CD_ERROR_NOT_FOUND : LOM_NATIVE_CD_ERROR_IO;
        return NULL;
    }

    file = fopen(path, "rb");

    if (file == NULL)
    {
        g_last_error = (errno == ENOENT) ? LOM_NATIVE_CD_ERROR_NOT_FOUND : LOM_NATIVE_CD_ERROR_IO;
        return NULL;
    }

    data = (u8*)malloc((size_t)size);

    if (data == NULL)
    {
        fclose(file);
        g_last_error = LOM_NATIVE_CD_ERROR_IO;
        return NULL;
    }

    bytes_read = fread(data, 1, (size_t)size, file);

    if (fclose(file) != 0 || bytes_read != (size_t)size)
    {
        free(data);
        g_last_error = LOM_NATIVE_CD_ERROR_IO;
        return NULL;
    }

    *size_out = size;
    g_last_error = LOM_NATIVE_CD_ERROR_NONE;
    return data;
}

/**
 * @brief Read a whole resource's file to where the game asked, and set the last
 *        error.
 * @param resource_index The resource.
 * @param dst_buffer Where to read it to.
 * @return Its size, or -1.
 */
static s32 read_resource_file(int resource_index, void* dst_buffer)
{
    char path[LOM_NATIVE_CD_PATH_MAX];
    FILE* file;
    s32 size;
    size_t bytes_read;

    if (dst_buffer == NULL)
    {
        g_last_error = LOM_NATIVE_CD_ERROR_IO;
        return -1;
    }

    if (lom_native_cd_resolve_resource_path(resource_index, path, sizeof(path)) != 0)
    {
        return -1;
    }

    size = get_file_size(path);

    if (size < 0)
    {
        g_last_error = (errno == ENOENT) ? LOM_NATIVE_CD_ERROR_NOT_FOUND : LOM_NATIVE_CD_ERROR_IO;
        return -1;
    }

    if (!destination_fits(dst_buffer, (size_t)size))
    {
        g_last_error = LOM_NATIVE_CD_ERROR_INVALID_DESTINATION;
        return -1;
    }

    file = fopen(path, "rb");

    if (file == NULL)
    {
        g_last_error = (errno == ENOENT) ? LOM_NATIVE_CD_ERROR_NOT_FOUND : LOM_NATIVE_CD_ERROR_IO;
        return -1;
    }

    bytes_read = fread(dst_buffer, 1, (size_t)size, file);

    if (fclose(file) != 0 || bytes_read != (size_t)size)
    {
        g_last_error = LOM_NATIVE_CD_ERROR_IO;
        return -1;
    }

    g_last_error = LOM_NATIVE_CD_ERROR_NONE;
    return size;
}

int lom_native_cd_set_disc_root(const char* path)
{
    size_t length;

    if (path == NULL || path[0] == '\0')
    {
        return -1;
    }

    length = strlen(path);

    while (length > 1 && path[length - 1] == '/')
    {
        length--;
    }

    if (length >= sizeof(g_disc_root))
    {
        return -1;
    }

    memcpy(g_disc_root, path, length);
    g_disc_root[length] = '\0';
    g_disc_root_explicit = 1;
    return 0;
}

const char* lom_native_cd_get_disc_root(void)
{
    return g_disc_root;
}

int lom_native_cd_resolve_resource_path(int resource_index, char* path, size_t path_size)
{
    const char* relative_path;
    int written;

    relative_path = find_resource_relative_path(resource_index);

    if (relative_path == NULL)
    {
        fprintf(stderr, "[lom-native] CD resource %04X is not mapped\n", (unsigned)resource_index);
        g_last_error = LOM_NATIVE_CD_ERROR_INVALID_RESOURCE;
        return -1;
    }

    written = snprintf(path, path_size, "%s/%s", g_disc_root, relative_path);

    if (written < 0 || (size_t)written >= path_size)
    {
        g_last_error = LOM_NATIVE_CD_ERROR_IO;
        return -1;
    }

    return 0;
}

LomNativeCdError lom_native_cd_get_last_error(void)
{
    return g_last_error;
}

void cdrom_init(void)
{
    const char* env_path;
    cancel_callback_reads();

    if (!g_disc_root_explicit)
    {
        env_path = lom_native_env_value(LOM_ENV_DISC_PATH);

        if (env_path != NULL)
        {
            lom_native_cd_set_disc_root(env_path);
        }
    }

    g_last_error = LOM_NATIVE_CD_ERROR_NONE;
}

void cdrom_stop(void)
{
    cancel_callback_reads();
    g_last_error = LOM_NATIVE_CD_ERROR_NONE;
}

/**
 * @brief Work out how long a compressed resource is once it's decompressed,
 *        checking every command fits and no copy reaches back before the start.
 * @param data The compressed resource, with its one-byte header.
 * @param bytes Bytes at @p data.
 * @param length Set to its decompressed length.
 * @return 0, or -1 when it's cut short, bad, or bigger than the PS1's RAM.
 */
static int decompressed_length(const u8* data, size_t bytes, size_t* length)
{
    size_t position = 1, produced = 0;

    while (position < bytes)
    {
        unsigned op = data[position];
        size_t input = 1, output = 0, distance = 0;

        if (op == 0xFF)
        {
            *length = produced;
            return 0;
        }

        if (op < 0xF0)
        {
            output = op + 1;
            input += output;
        }
        else
        {
            if (bytes - position < 2)
            {
                return -1;
            }

            unsigned count = data[position + 1];

            switch (op)
            {
            case 0xF0:
                input = 2;
                output = (count & 15) + 3;
                break;
            case 0xF1:
                input = 3;
                output = count + 4;
                break;
            case 0xF2:
                input = 3;
                output = (count + 2) * 2;
                break;
            case 0xF3:
                input = 4;
                output = (count + 2) * 2;
                break;
            case 0xF4:
                input = 5;
                output = (count + 2) * 3;
                break;
            case 0xF5:
                input = 3 + count + 4;
                output = (count + 4) * 2;
                break;
            case 0xF6:
                input = 4 + count + 3;
                output = (count + 3) * 3;
                break;
            case 0xF7:
                input = 5 + count + 2;
                output = (count + 2) * 4;
                break;
            case 0xF8:
            case 0xF9:
                input = 3;
                output = count + 4;
                break;
            case 0xFA:
                input = 4;
                output = count + 5;
                break;
            case 0xFB:
                input = 5;
                output = (count + 3) * 2;
                break;
            case 0xFC:
                if (bytes - position < 3)
                {
                    return -1;
                }

                input = 3;
                output = (data[position + 2] >> 4) + 4;
                distance = count + ((data[position + 2] & 15) << 8) + 1;
                break;
            case 0xFD:
                if (bytes - position < 3)
                {
                    return -1;
                }

                input = 3;
                output = data[position + 2] + 20;
                distance = count + 1;
                break;
            case 0xFE:
                input = 2;
                output = (count & 15) + 3;
                distance = ((count & 0xF0) >> 1) + 8;
                break;
            }
        }

        if (input > bytes - position || distance > produced || output > LOM_PS1_RAM_SIZE - produced)
        {
            return -1;
        }

        position += input;
        produced += output;
    }

    return -1;
}

#define STREAM_RAM_START ((uintptr_t)lom_native_ram)
#define STREAM_RAM_END ((uintptr_t)lom_native_ram + LOM_PS1_RAM_SIZE * MEMORY_MAP_SCALE)

s32 lom_native_cd_stream(s32 resource_index, u8* destination)
{
    uintptr_t destination_address;
    uintptr_t ram_end_address;
    u8* compressed_data;
    u8* source;
    u8* source_end;
    u8* destination_start;
    u8* destination_cursor;
    u8* destination_end;
    s32 compressed_size;
    s32 decompress_result;
    s32 decompressed_size;
    size_t expected_size;

    if (drain_callback_reads())
    {
        return -1;
    }

    destination_address = (uintptr_t)destination;
    ram_end_address = STREAM_RAM_END;

    if (destination_address < STREAM_RAM_START || destination_address >= ram_end_address)
    {
        size_t left = lom_unit_host_bytes_left(destination);

        if (left == 0)
        {
            g_last_error = LOM_NATIVE_CD_ERROR_INVALID_DESTINATION;
            return -1;
        }

        ram_end_address = destination_address + left;
    }

    compressed_data = load_resource_file(resource_index, &compressed_size);

    if (compressed_data == NULL)
    {
        return -1;
    }

    if (decompressed_length(compressed_data, (size_t)compressed_size, &expected_size) || expected_size > ram_end_address - destination_address)
    {
        free(compressed_data);
        g_last_error = LOM_NATIVE_CD_ERROR_DECOMPRESSION;
        return -1;
    }

    source = compressed_data + 1;
    source_end = compressed_data + compressed_size;
    destination_start = (u8*)destination_address;
    destination_cursor = destination_start;
    destination_end = (u8*)(ram_end_address + 1);

    decompress_result = cdrom_decompress_data(&source, &destination_cursor, source_end, destination_end);

    free(compressed_data);

    if (decompress_result != 0 || (size_t)(destination_cursor - destination_start) != expected_size)
    {
        g_last_error = LOM_NATIVE_CD_ERROR_DECOMPRESSION;
        return -1;
    }

    decompressed_size = (s32)(destination_cursor - destination_start);
    g_last_error = LOM_NATIVE_CD_ERROR_NONE;
    lom_native_unit_streamed(resource_index, destination_start, decompressed_size);
    return decompressed_size;
}

/**
 * @brief Load a compressed resource and decompress it into memory of its own.
 * @param resource_index The resource.
 * @param size Set to its decompressed size.
 * @return The bytes, to free, or NULL with g_last_error set.
 */
static u8* load_decompressed(u16 resource_index, size_t* size)
{
    s32 compressed_size;
    u8* compressed = load_resource_file(resource_index, &compressed_size);

    if (!compressed)
    {
        return NULL;
    }

    if (decompressed_length(compressed, (size_t)compressed_size, size))
    {
        free(compressed);
        g_last_error = LOM_NATIVE_CD_ERROR_DECOMPRESSION;
        return NULL;
    }

    u8* decoded = malloc(*size + 1);

    if (!decoded)
    {
        free(compressed);
        g_last_error = LOM_NATIVE_CD_ERROR_IO;
        return NULL;
    }

    u8* source = compressed + 1;
    u8* output = decoded;
    s32 result = cdrom_decompress_data(&source, &output, compressed + compressed_size, decoded + *size + 1);
    free(compressed);

    if (result || (size_t)(output - decoded) != *size)
    {
        free(decoded);
        g_last_error = LOM_NATIVE_CD_ERROR_DECOMPRESSION;
        return NULL;
    }

    return decoded;
}

void cdrom_stream_chunked(u16 resource_index, CdStreamGetBufferCallback get_buffer, CdStreamChunkDoneCallback chunk_done)
{
    lom_native_boundary_event(LOM_BOUNDARY_FILES, LOM_BOUNDARY_FILE_CHUNKED, resource_index, 0);

    if (drain_callback_reads())
    {
        return;
    }

    if (!get_buffer || !chunk_done)
    {
        g_last_error = LOM_NATIVE_CD_ERROR_INVALID_DESTINATION;
        return;
    }

    size_t size;
    u8* decoded = load_decompressed(resource_index, &size);

    if (!decoded)
    {
        return;
    }

    size_t delivered = 0;
    s32 chunk = 0;

    do
    {
        s32 capacity = 0;
        u8* destination = get_buffer((s32)delivered, &capacity);
        size_t count = capacity == -1 ? size - delivered : capacity > 0 ? (size_t)capacity : 0;

        if (count > size - delivered)
        {
            count = size - delivered;
        }

        if (!destination || (capacity != -1 && capacity <= 0) || !destination_fits(destination, count))
        {
            free(decoded);
            g_last_error = LOM_NATIVE_CD_ERROR_INVALID_DESTINATION;
            return;
        }

        memcpy(destination, decoded + delivered, count);
        delivered += count;
        chunk_done(chunk++);
    } while (delivered < size);

    free(decoded);
    g_last_error = LOM_NATIVE_CD_ERROR_NONE;
}

/**
 * @brief Check whether a read is the same as the last one queued, which the
 *        game asks for again while it waits.
 */
static int repeats_last_read(u8 command, s32 resource_index, void* dst_buffer, CdCommandCallback callback)
{
    if (!g_callback_count)
    {
        return 0;
    }

    const NativeCallbackRead* last = &g_callback_reads[(g_callback_head + g_callback_count - 1) % CALLBACK_READS];
    return last->resource == resource_index && last->callback == callback && last->destination == dst_buffer && last->command == command;
}

/**
 * @brief Queue a read or seek for cdrom_process_state to finish over the next
 *        frames, as the drive would. A read with a callback gets its
 *        destination from it; one without goes to @p dst_buffer.
 * @param command CdlReadN or CdlSeekL.
 * @param resource_index The resource.
 * @param path Its file.
 * @param size Its size.
 * @param dst_buffer Where a read without a callback goes.
 * @param callback The callback, or NULL.
 * @return The size, or -1 or -2 with g_last_error set.
 */
static s32 queue_callback_read(u8 command, s32 resource_index, const char* path, s32 size, void* dst_buffer, CdCommandCallback callback)
{
    if (size <= 0)
    {
        g_last_error = LOM_NATIVE_CD_ERROR_INVALID_RESOURCE;
        return -2;
    }

    if (repeats_last_read(command, resource_index, dst_buffer, callback))
    {
        g_last_error = LOM_NATIVE_CD_ERROR_NONE;
        return size;
    }

    if (g_callback_count == CALLBACK_READS)
    {
        g_last_error = LOM_NATIVE_CD_ERROR_QUEUE_FULL;
        return -1;
    }

    FILE* file = command == CdlSeekL ? NULL : fopen(path, "rb");

    if (command != CdlSeekL && !file)
    {
        g_last_error = errno == ENOENT ? LOM_NATIVE_CD_ERROR_NOT_FOUND : LOM_NATIVE_CD_ERROR_IO;
        return -2;
    }

    if (command == CdlReadN && !callback && !destination_fits(dst_buffer, (size_t)size))
    {
        fclose(file);
        g_last_error = LOM_NATIVE_CD_ERROR_INVALID_DESTINATION;
        return -2;
    }

    g_callback_reads[(g_callback_head + g_callback_count) % CALLBACK_READS] = (NativeCallbackRead){.file = file,
                                                                                                   .callback = callback,
                                                                                                   .destination = dst_buffer,
                                                                                                   .resource = resource_index,
                                                                                                   .size = size,
                                                                                                   .command = command,
                                                                                                   .ticket = ++g_callback_next_ticket};
    ++g_callback_count;
    g_last_error = LOM_NATIVE_CD_ERROR_NONE;
    return size;
}

s32 cdrom_queue_command(u8 command, s32 resource_index, void* dst_buffer, CdCommandCallback callback)
{
    char path[LOM_NATIVE_CD_PATH_MAX];
    s32 size;

    resource_index = (u16)resource_index;

    if (resource_index == CD_RESOURCE_INDEX_DEFAULT)
    {
        g_last_error = LOM_NATIVE_CD_ERROR_UNSUPPORTED;
        return -1;
    }

    if (lom_native_cd_resolve_resource_path(resource_index, path, sizeof(path)) != 0)
    {
        return -2;
    }

    size = get_file_size(path);

    if (size < 0)
    {
        g_last_error = (errno == ENOENT) ? LOM_NATIVE_CD_ERROR_NOT_FOUND : LOM_NATIVE_CD_ERROR_IO;
        return -2;
    }

    if (command == CdlSeekL && !g_callback_count)
    {
        g_last_error = LOM_NATIVE_CD_ERROR_NONE;
        return size;
    }

    if (command != CdlReadN && command != CdlSeekL)
    {
        g_last_error = LOM_NATIVE_CD_ERROR_UNSUPPORTED;
        return -1;
    }

    if (callback || g_callback_count)
    {
        return queue_callback_read(command, resource_index, path, size, dst_buffer, callback);
    }

    return read_resource_file(resource_index, dst_buffer);
}

u32 cdrom_process_state(void)
{
    if (!g_callback_count || g_callback_pumping)
    {
        return g_callback_count;
    }

    g_callback_pumping = 1;
    NativeCallbackRead* read = &g_callback_reads[g_callback_head];
    unsigned generation = g_callback_generation;
    uint64_t ticket = read->ticket;
    size_t remaining = (size_t)(read->size - read->delivered);

    if (read->command == CdlSeekL)
    {
        memset(read, 0, sizeof(*read));
        g_callback_head = (g_callback_head + 1) % CALLBACK_READS;
        --g_callback_count;
        g_callback_pumping = 0;
        g_last_error = LOM_NATIVE_CD_ERROR_NONE;
        return g_callback_count;
    }

    u8* buffer = read->callback ? read->callback(read->delivered, (u32)remaining) : read->destination + read->delivered;

    if (generation != g_callback_generation)
    {
        if (!g_callback_count || g_callback_reads[g_callback_head].ticket != ticket)
        {
            g_callback_pumping = 0;
            return g_callback_count;
        }

        read = &g_callback_reads[g_callback_head];
    }

    if (!buffer)
    {
        g_callback_pumping = 0;
        return g_callback_count;
    }

    size_t bytes = remaining > 2048 ? 2048 : remaining;
    size_t padded = read->callback ? (bytes + 3) & ~(size_t)3 : bytes;
    int failed = !destination_fits(buffer, padded);

    if (failed)
    {
        g_last_error = LOM_NATIVE_CD_ERROR_INVALID_DESTINATION;
    }
    else if (fread(buffer, 1, bytes, read->file) != bytes)
    {
        failed = 1;
        g_last_error = LOM_NATIVE_CD_ERROR_IO;
    }
    else
    {
        memset(buffer + bytes, 0, padded - bytes);
        read->delivered += (s32)bytes;
        g_last_error = LOM_NATIVE_CD_ERROR_NONE;
    }

    if (failed || read->delivered == read->size)
    {
        if (fclose(read->file) && !failed)
        {
            g_last_error = LOM_NATIVE_CD_ERROR_IO;
        }

        memset(read, 0, sizeof(*read));
        g_callback_head = (g_callback_head + 1) % CALLBACK_READS;
        --g_callback_count;
    }

    g_callback_pumping = 0;
    return g_callback_count;
}

void cdrom_verify_recovery(void)
{
}

void lom_native_cd_wait(void)
{
    (void)drain_callback_reads();
}

void cdrom_load_resource_table(s32 lba, s32 data_size_bytes)
{
    (void)lba;
    (void)data_size_bytes;

    g_last_error = LOM_NATIVE_CD_ERROR_NONE;
}

void cdrom_reset(void)
{
    cancel_callback_reads();
    g_last_error = LOM_NATIVE_CD_ERROR_NONE;
}

s32 lom_native_cd_queue_read(s32 resource_index, void* dst_buffer)
{
    return cdrom_queue_command(CdlReadN, (u16)resource_index, dst_buffer, NULL);
}

static const LomNativeCdHooks* g_hooks;

void lom_native_cd_set_hooks(const LomNativeCdHooks* hooks)
{
    g_hooks = hooks;
}

static LomNativeCdReplacement g_replacement;

void lom_native_cd_set_replacement(LomNativeCdReplacement replacement)
{
    g_replacement = replacement;
}

/**
 * @brief Hand a file the game read to the replacement hook, which can change it
 *        in place, like putting in the Remaster's text.
 * @param resource_index The resource.
 * @param destination Where it was read to.
 * @param result The read's result, its size when it worked.
 * @return Its size after any change, or @p result when it failed or nothing's
 *        hooked.
 */
static s32 replace(s32 resource_index, void* destination, s32 result)
{
    if (result <= 0 || !g_replacement || !destination)
    {
        return result;
    }

    uintptr_t address = (uintptr_t)destination;
    int in_ram = address >= STREAM_RAM_START && address < STREAM_RAM_END;
    size_t room = in_ram ? (size_t)(STREAM_RAM_END - address) : lom_unit_host_bytes_left(destination);
    return g_replacement((u16)resource_index, (unsigned char*)destination, result, room);
}

s32 cdrom_queue_read(s32 resource_index, void* dst_buffer)
{
    const LomNativeCdHooks* hooks = g_hooks;

    if (hooks && hooks->before)
    {
        hooks->before(LOM_NATIVE_CD_CALL_READ, resource_index);
    }

    const int reads_now = g_callback_count == 0;
    s32 result = lom_native_cd_queue_read(resource_index, dst_buffer);

    if (reads_now)
    {
        result = replace(resource_index, dst_buffer, result);
    }

    lom_native_boundary_event(LOM_BOUNDARY_FILES, LOM_BOUNDARY_FILE_READ, (u16)resource_index, (unsigned)result);

    if (hooks && hooks->after)
    {
        hooks->after(LOM_NATIVE_CD_CALL_READ, resource_index, result);
    }

    return result;
}

const char* lom_native_cd_resource_name(int resource_index)
{
    return find_resource_relative_path(resource_index);
}

s32 cdrom_stream(s32 resource_index, u8* destination)
{
    const LomNativeCdHooks* hooks = g_hooks;

    if (hooks && hooks->before)
    {
        hooks->before(LOM_NATIVE_CD_CALL_STREAM, resource_index);
    }

    s32 result = lom_native_cd_stream(resource_index, destination);

    result = replace(resource_index, destination, result);
    lom_native_boundary_event(LOM_BOUNDARY_FILES, LOM_BOUNDARY_FILE_STREAM, (u16)resource_index, (unsigned)result);

    if (hooks && hooks->after)
    {
        hooks->after(LOM_NATIVE_CD_CALL_STREAM, resource_index, result);
    }

    return result;
}

void cdrom_wait_queue_empty(void)
{
    const LomNativeCdHooks* hooks = g_hooks;

    if (hooks && hooks->before)
    {
        hooks->before(LOM_NATIVE_CD_CALL_WAIT, 0);
    }

    lom_native_cd_wait();

    if (hooks && hooks->after)
    {
        hooks->after(LOM_NATIVE_CD_CALL_WAIT, 0, 0);
    }
}

s32 cdrom_can_queue_resource(s32 resource_index)
{
    resource_index = (u16)resource_index;

    for (unsigned i = 0; i < g_callback_count; ++i)
    {
        if (g_callback_reads[(g_callback_head + i) % CALLBACK_READS].resource == resource_index)
        {
            return 0;
        }
    }

    return 1;
}

s32 cdrom_queue_read_with_callback(s32 resource_index, CdCommandCallback callback)
{
    s32 result = cdrom_queue_command(CdlReadN, (u16)resource_index, NULL, callback);
    lom_native_boundary_event(LOM_BOUNDARY_FILES, LOM_BOUNDARY_FILE_CALLBACK_READ, (u16)resource_index, (unsigned)result);
    return result;
}

s32 cdrom_queue_seek(s32 resource_index)
{
    s32 result = cdrom_queue_command(CdlSeekL, (u16)resource_index, NULL, NULL);
    lom_native_boundary_event(LOM_BOUNDARY_FILES, LOM_BOUNDARY_FILE_SEEK, (u16)resource_index, (unsigned)result);
    return result;
}

s32 cdrom_get_resource_size(s32 resource_index)
{
    u16 table_index = (u16)resource_index;
    size_t i;

    for (i = 0; i < sizeof(g_resource_paths) / sizeof(g_resource_paths[0]); i++)
    {
        if (g_resource_paths[i].resource_index == table_index)
        {
            return g_resource_paths[i].data_size;
        }
    }

    return 0;
}

s32 cdrom_get_error_status(void)
{
    return g_last_error == LOM_NATIVE_CD_ERROR_NONE ? 0 : 1;
}

void cdrom_defer_data_ready(void)
{
}
