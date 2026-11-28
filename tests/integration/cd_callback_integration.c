#define _GNU_SOURCE
#define u_long LomHostULong
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#undef u_long
#include "common.h"
#include "main/cdrom.h"
#include "main/cd_resources.h"
#include "lom_native_cd.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_fs.h"
#include "../support/platform/lom_test_posix.h"
#include "../support/lom_check.h"

#define RAM(address) ((u8*)MEMORY_MAP_RAM(address))
#define RAM_END ((u8*)lom_native_ram + LOM_PS1_RAM_SIZE * MEMORY_MAP_SCALE)
#define RAM_LAST (RAM_END - 1)
static u8 received[16384];

static unsigned calls, accepted, ready, pause_read, reset_read, recursive_read, enqueue_read;

static unsigned selective_cancel, cancel_later;

static u8* other_sector(s32 total, u32 remaining);

static u8 enqueued_data[8192];

static s32 totals[16];

static u32 remains[16];

/**
 * @brief The game's sector callback, for 0xB1: take one sector at a time and
 *        note where it went, or do what the case asks partway through (reset
 *        the CD, cancel a read, read again from inside, queue another read, or
 *        hold the sector back).
 * @param total Bytes delivered so far.
 * @param remaining Bytes still to come.
 * @return Where the next sector goes, or NULL to hold it.
 */
static u8* sector(s32 total, u32 remaining)
{
    ++calls;

    if (reset_read)
    {
        cdrom_reset();
        return received;
    }

    if (selective_cancel)
    {
        CHECK(lom_native_cd_cancel_callback(0xB1, sector) == 1);
        return received;
    }

    if (cancel_later)
    {
        cancel_later = 0;
        CHECK(lom_native_cd_cancel_callback(0xB2, other_sector) == 1);
    }

    if (recursive_read)
    {
        CHECK(cdrom_process_state() == 1);
    }

    if (pause_read || ready)
    {
        return NULL;
    }

    if (enqueue_read)
    {
        enqueue_read = 0;
        CHECK(cdrom_queue_read(0xB2, enqueued_data) == 2013);
    }

    CHECK(accepted < 16 && (u32)total + remaining < sizeof(received));
    totals[accepted] = total;
    remains[accepted++] = remaining;
    ready = 1;
    return received + total;
}

/**
 * @brief A second read's sector callback, for 0xB2, which takes every sector.
 * @param total Bytes delivered so far.
 * @param remaining Bytes still to come.
 * @return Where the next sector goes.
 */
static u8* other_sector(s32 total, u32 remaining)
{
    CHECK(remaining > 0 && total >= 0);
    return received + total;
}

/**
 * @brief Reset the CD and the cases' state.
 */
static void reset(void)
{
    cdrom_reset();
    memset(received, 0xA5, sizeof(received));
    calls = accepted = ready = pause_read = reset_read = recursive_read = enqueue_read = 0;
    selective_cancel = cancel_later = 0;
}

/**
 * @brief Read a file from the extracted disc as it is.
 * @param disc The disc's folder.
 * @param name The file, from the disc's root.
 * @param size Set to its size.
 * @return Its bytes, for the caller to free.
 */
static u8* raw(const char* disc, const char* name, size_t* size)
{
    char path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/%s", disc, name) < (int)sizeof(path));
    FILE* file = fopen(path, "rb");
    CHECK(file);
    CHECK(!fseek(file, 0, SEEK_END));
    long length = ftell(file);
    CHECK(length > 0);
    rewind(file);
    u8* data = malloc((size_t)length);
    CHECK(data);
    CHECK(fread(data, 1, (size_t)length, file) == (size_t)length);
    CHECK(!fclose(file));
    *size = (size_t)length;
    return data;
}

/**
 * @brief Check callback reads deliver the file a sector at a time, wait when
 *        the callback holds a sector, and survive a reset, a cancel or a read
 *        from inside the callback.
 * @param disc The disc's folder.
 */
static void callback_cases(const char* disc)
{
    size_t size;
    u8* expected = raw(disc, "MAP/FDATA/M_WIN.PRS", &size);
    CHECK(size == 2785);
    reset();
    CHECK(cdrom_queue_read_with_callback(0xB1, sector) == (s32)size);
    CHECK(!calls && !cdrom_can_queue_resource(0xB1));
    CHECK(cdrom_queue_read_with_callback(0xB1, sector) == (s32)size);
    pause_read = 1;
    CHECK(cdrom_process_state() == 1 && calls == 1 && !accepted && received[0] == 0xA5);
    pause_read = 0;
    CHECK(cdrom_process_state() == 1 && accepted == 1 && ready);
    CHECK(totals[0] == 0 && remains[0] == size && !memcmp(received, expected, 2048));
    CHECK(cdrom_process_state() == 1 && accepted == 1);
    ready = 0;
    CHECK(cdrom_process_state() == 0 && accepted == 2);
    CHECK(totals[1] == 2048 && remains[1] == size - 2048 && !memcmp(received, expected, size));
    CHECK(received[size] == 0 && received[size + 1] == 0 && received[size + 2] == 0 && received[size + 3] == 0xA5);
    CHECK(cdrom_can_queue_resource(0xB1) && !cdrom_get_error_status());
    reset();
    CHECK(cdrom_queue_read_with_callback(0xB1, sector) == (s32)size);
    static u8 immediate[8192];
    memset(immediate, 0xA5, sizeof(immediate));
    CHECK(cdrom_queue_read(0xB1, immediate) == (s32)size);
    pause_read = 1;
    CHECK(cdrom_process_state() == 2 && immediate[0] == 0xA5);
    cdrom_wait_queue_empty();
    CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_CALLBACK_BLOCKED && immediate[0] == 0xA5);
    CHECK(cdrom_stream(CD_RES_FIELD_FONT, RAM(0x80050000)) == -1 && lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_CALLBACK_BLOCKED);
    pause_read = 0;
    CHECK(cdrom_process_state() == 2 && immediate[0] == 0xA5);
    ready = 0;
    CHECK(cdrom_process_state() == 1 && immediate[0] == 0xA5);
    cdrom_wait_queue_empty();
    CHECK(!cdrom_process_state() && !memcmp(immediate, expected, size));
    memset(immediate, 0xA5, sizeof(immediate));
    CHECK(cdrom_queue_read(0xB1, immediate) == (s32)size);
    CHECK(!memcmp(immediate, expected, size));
    cdrom_stop();
    CHECK(!cdrom_process_state());
    reset();
    recursive_read = 1;
    CHECK(cdrom_queue_read_with_callback(0xB1, sector) == (s32)size);
    CHECK(cdrom_process_state() == 1 && accepted == 1);
    ready = 0;
    CHECK(!cdrom_process_state());
    reset();
    reset_read = 1;
    CHECK(cdrom_queue_read_with_callback(0xB1, sector) == (s32)size);
    CHECK(!cdrom_process_state() && received[0] == 0xA5);
    reset();
    CHECK(cdrom_queue_read_with_callback(0xB1, other_sector) == (s32)size);
    CHECK(cdrom_queue_read_with_callback(0xB2, other_sector) == 2013);
    cdrom_wait_queue_empty();
    CHECK(!cdrom_process_state() && !cdrom_get_error_status());
    size_t font_size;
    u8* font = raw(disc, "MAP/FDATA/FONT.PRS", &font_size);
    CHECK(font_size == 2013 && !memcmp(received, font, font_size));
    reset();
    enqueue_read = 1;
    memset(enqueued_data, 0xA5, sizeof(enqueued_data));
    CHECK(cdrom_queue_read_with_callback(0xB1, sector) == (s32)size);
    CHECK(cdrom_process_state() == 2 && enqueued_data[0] == 0xA5);
    ready = 0;
    CHECK(cdrom_process_state() == 1 && enqueued_data[0] == 0xA5);
    cdrom_wait_queue_empty();
    CHECK(!cdrom_process_state() && !memcmp(enqueued_data, font, font_size));
    free(font);
    reset();
    unsigned epoch = lom_native_cd_reset_epoch();
    CHECK(cdrom_queue_read_with_callback(0xB1, sector) == (s32)size);
    memset(immediate, 0xA5, sizeof(immediate));
    CHECK(cdrom_queue_read(0xB1, immediate) == (s32)size);
    CHECK(cdrom_queue_read_with_callback(0xB1, other_sector) == (s32)size);
    CHECK(lom_native_cd_callback_pending(0xB1, sector));
    CHECK(lom_native_cd_cancel_callback(0xB1, sector) == 1);
    CHECK(!lom_native_cd_callback_pending(0xB1, sector));
    CHECK(lom_native_cd_callback_pending(0xB1, other_sector));
    CHECK(lom_native_cd_reset_epoch() == epoch);
    CHECK(cdrom_process_state() == 2 && !memcmp(immediate, expected, 2048));
    CHECK(lom_native_cd_cancel_callback(0xB1, other_sector) == 1);
    cdrom_wait_queue_empty();
    CHECK(!memcmp(immediate, expected, size));
    CHECK(!lom_native_cd_cancel_callback(0xB1, other_sector));
    reset();
    selective_cancel = 1;
    memset(immediate, 0xA5, sizeof(immediate));
    CHECK(cdrom_queue_read_with_callback(0xB1, sector) == (s32)size);
    CHECK(cdrom_queue_read(0xB1, immediate) == (s32)size);
    CHECK(cdrom_process_state() == 1 && received[0] == 0xA5 && immediate[0] == 0xA5);
    cdrom_wait_queue_empty();
    CHECK(!memcmp(immediate, expected, size));
    reset();
    cancel_later = 1;
    memset(immediate, 0xA5, sizeof(immediate));
    CHECK(cdrom_queue_read_with_callback(0xB1, sector) == (s32)size);
    CHECK(cdrom_queue_read_with_callback(0xB2, other_sector) == 2013);
    CHECK(cdrom_queue_read(0xB2, immediate) == 2013);
    CHECK(cdrom_process_state() == 2 && accepted == 1 && ready);
    CHECK(!memcmp(received, expected, 2048) && immediate[0] == 0xA5);
    ready = 0;
    CHECK(cdrom_process_state() == 1 && accepted == 2);
    CHECK(!memcmp(received, expected, size));
    cdrom_wait_queue_empty();
    size_t retained_size;
    u8* retained = raw(disc, "MAP/FDATA/FONT.PRS", &retained_size);
    CHECK(!memcmp(immediate, retained, retained_size));
    free(retained);
    epoch = lom_native_cd_reset_epoch();
    cdrom_stop();
    CHECK(lom_native_cd_reset_epoch() != epoch);
    reset();

    for (unsigned i = 0; i < 15; ++i)
    {
        CHECK(cdrom_queue_read_with_callback(i & 1 ? 0xB2 : 0xB1, sector) > 0);
    }

    CHECK(cdrom_queue_read_with_callback(0xB2, sector) == -1 && lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_QUEUE_FULL);
    cdrom_reset();
    CHECK(!cdrom_process_state() && cdrom_can_queue_resource(0xB1));
    free(expected);
}

static const u8* reference;

static size_t reference_size, delivered, chunk_start;

static s32 chunk_capacity, last_chunk;

static unsigned chunk_calls, done_calls;

static u8 chunk_buffer[4113];

/**
 * @brief Hand the chunked stream the next buffer, of the size the case is
 *        testing, or RAM itself for -1.
 * @param total Bytes delivered so far.
 * @param capacity Set to the buffer's size.
 * @return The buffer.
 */
static u8* chunk_buffer_callback(s32 total, s32* capacity)
{
    CHECK((size_t)total == delivered);
    ++chunk_calls;
    chunk_start = delivered;
    *capacity = chunk_capacity;
    memset(chunk_buffer, 0xA5, sizeof(chunk_buffer));

    if (chunk_capacity == -1)
    {
        return RAM(0x80100000);
    }

    return chunk_buffer + 1;
}

/**
 * @brief Check a chunk of the stream matches the file, and arrived in order.
 * @param index The chunk's number.
 */
static void chunk_done(s32 index)
{
    CHECK(index == last_chunk++);
    ++done_calls;
    size_t count = chunk_capacity == -1 ? reference_size - delivered : (size_t)chunk_capacity;

    if (count > reference_size - delivered)
    {
        count = reference_size - delivered;
    }

    u8* output = chunk_capacity == -1 ? RAM(0x80100000) : chunk_buffer + 1;
    CHECK(!memcmp(output, reference + chunk_start, count));

    if (chunk_capacity != -1)
    {
        CHECK(chunk_buffer[0] == 0xA5 && chunk_buffer[count + 1] == 0xA5);
    }

    delivered += count;
}

/**
 * @brief Check chunked streams of two files, with buffers of several sizes,
 *        give the same bytes as a plain stream.
 */
static void chunk_cases(void)
{
    const unsigned resources[] = {CD_RES_FIELD_FONT, CD_RES_TITLE_BIN};
    const s32 capacities[] = {1, 3, 7, 64, 1024, 4096, -1};

    for (unsigned r = 0; r < 2; ++r)
    {
        s32 size = cdrom_stream(resources[r], RAM(0x80050000));
        CHECK(size > 0 && size < 0x100000);
        u8* expected = malloc((size_t)size);
        CHECK(expected);
        memcpy(expected, RAM(0x80050000), (size_t)size);
        reference = expected;
        reference_size = (size_t)size;

        for (unsigned c = 0; c < 7; ++c)
        {
            chunk_capacity = capacities[c];
            delivered = chunk_calls = done_calls = 0;
            last_chunk = 0;
            cdrom_stream_chunked((u16)resources[r], chunk_buffer_callback, chunk_done);
            CHECK(!cdrom_get_error_status() && delivered == reference_size && chunk_calls == done_calls);
            size_t wanted = chunk_capacity == -1 ? 1 : (reference_size + (size_t)chunk_capacity - 1) / (size_t)chunk_capacity;
            CHECK(done_calls == wanted);
        }

        free(expected);
    }
}

/**
 * @brief A chunk buffer with no room, which the stream must refuse.
 * @param total Ignored.
 * @param capacity Set to 0.
 * @return A buffer.
 */
static u8* bad_buffer(s32 total, s32* capacity)
{
    (void)total;
    *capacity = 0;
    return received;
}

/**
 * @brief A sector callback that points at the last byte of RAM, where a sector
 *        doesn't fit.
 * @param total Ignored.
 * @param remaining Ignored.
 * @return The last byte of RAM.
 */
static u8* short_ram(s32 total, u32 remaining)
{
    (void)total;
    (void)remaining;
    return RAM_LAST;
}

static uintptr_t selected_address;

/**
 * @brief A sector callback that reads to the address the case picked.
 * @param total Bytes delivered so far.
 * @param remaining Ignored.
 * @return Where the next sector goes.
 */
static u8* selected_ram(s32 total, u32 remaining)
{
    (void)remaining;
    return (void*)(selected_address + (uintptr_t)total);
}

/**
 * @brief Check streams of malformed compressed files fail cleanly, and reads to
 *        places they don't fit are refused.
 * @param disc The disc's folder.
 */
static void malformed_cases(const char* disc)
{
    char root[4096];
    CHECK(lom_test_make_temp_folder(root, sizeof(root), "lom-cd-callback"));
    char map[4096], data[4096], path[4096];
    CHECK(snprintf(map, sizeof(map), "%s/MAP", root) < (int)sizeof(map));
    CHECK(!lom_native_make_folder(map));
    CHECK(snprintf(data, sizeof(data), "%s/FDATA", map) < (int)sizeof(data));
    CHECK(!lom_native_make_folder(data));
    CHECK(snprintf(path, sizeof(path), "%s/FONT.PRS", data) < (int)sizeof(path));
    CHECK(!lom_native_cd_set_disc_root(root));
    const u8 malformed[][5] = {{0, 0xFC, 0, 0, 0xFF}, {0, 0xFD, 0, 0, 0xFF}, {0, 0xFE, 0, 0xFF, 0}, {0, 0xF4, 0xFF, 0, 0}, {0, 0, 'A', 0, 0}};
    const size_t sizes[] = {5, 5, 4, 3, 3};

    for (unsigned i = 0; i < 5; ++i)
    {
        FILE* file = fopen(path, "wb");
        CHECK(file);
        CHECK(fwrite(malformed[i], 1, sizes[i], file) == sizes[i]);
        CHECK(!fclose(file));
        chunk_calls = 0;
        cdrom_stream_chunked(0xB2, chunk_buffer_callback, chunk_done);
        CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_DECOMPRESSION && !chunk_calls);
        memset(RAM(0x80050000), 0xA5, 16);
        CHECK(cdrom_stream(0xB2, RAM(0x80050000)) == -1);
        CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_DECOMPRESSION);

        for (unsigned j = 0; j < 16; ++j)
        {
            CHECK(RAM(0x80050000)[j] == 0xA5);
        }
    }

    FILE* file = fopen(path, "wb");
    CHECK(file);
    fputc(0, file);

    for (unsigned i = 0; i < 8200; ++i)
    {
        const u8 block[] = {0xF1, 255, 'A'};
        CHECK(fwrite(block, 1, 3, file) == 3);
    }

    fputc(0xFF, file);
    CHECK(!fclose(file));
    chunk_calls = 0;
    cdrom_stream_chunked(0xB2, chunk_buffer_callback, chunk_done);
    CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_DECOMPRESSION && !chunk_calls);
    CHECK(cdrom_stream(0xB2, RAM(0x80050000)) == -1);
    CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_DECOMPRESSION);
    file = fopen(path, "wb");
    CHECK(file);
    const u8 small[] = {0, 0, 'Q', 0xFF};
    CHECK(fwrite(small, 1, sizeof(small), file) == sizeof(small));
    CHECK(!fclose(file));
    *RAM_LAST = 0xA5;
    CHECK(cdrom_stream(0xB2, RAM_LAST) == 1 && *RAM_LAST == 'Q');
    file = fopen(path, "wb");
    CHECK(file);
    const u8 too_large[] = {0, 1, 'Q', 'R', 0xFF};
    CHECK(fwrite(too_large, 1, sizeof(too_large), file) == sizeof(too_large));
    CHECK(!fclose(file));
    *RAM_LAST = 0xA5;
    CHECK(cdrom_stream(0xB2, RAM_LAST) == -1);
    CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_DECOMPRESSION && *RAM_LAST == 0xA5);
    file = fopen(path, "wb");
    CHECK(file);
    CHECK(fwrite(small, 1, sizeof(small), file) == sizeof(small));
    CHECK(!fclose(file));
    reset();
    CHECK(cdrom_queue_read_with_callback(0xB2, other_sector) == 4);
    file = fopen(path, "wb");
    CHECK(file);
    CHECK(!fclose(file));
    CHECK(!cdrom_process_state() && lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_IO);
    CHECK(!unlink(path));
    CHECK(cdrom_queue_read_with_callback(0xB2, sector) == -2);
    CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_NOT_FOUND);
    CHECK(!rmdir(data));
    CHECK(!rmdir(map));
    CHECK(!rmdir(root));
    CHECK(!lom_native_cd_set_disc_root(disc));
    cdrom_stream_chunked(0xB2, bad_buffer, chunk_done);
    CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_INVALID_DESTINATION);
    cdrom_stream_chunked(0xB2, NULL, chunk_done);
    CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_INVALID_DESTINATION);
    reset();
    CHECK(cdrom_queue_read_with_callback(0xB1, short_ram) > 0);
    CHECK(!cdrom_process_state() && lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_INVALID_DESTINATION);
    const uintptr_t ends[] = {(uintptr_t)RAM_END};
    const uintptr_t inside = (uintptr_t)RAM(0x80070000);

    for (unsigned i = 0; i < sizeof(ends) / sizeof(ends[0]); ++i)
    {
        reset();
        selected_address = ends[i];
        CHECK(cdrom_queue_read_with_callback(0xB2, selected_ram) == 2013);
        CHECK(!cdrom_process_state() && lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_INVALID_DESTINATION);
    }

    reset();
    selected_address = inside;
    CHECK(cdrom_queue_read_with_callback(0xB2, selected_ram) == 2013);
    cdrom_wait_queue_empty();
    CHECK(!cdrom_get_error_status());
    size_t size;
    u8* expected = raw(disc, "MAP/FDATA/FONT.PRS", &size);
    CHECK(!memcmp(RAM(0x80070000), expected, size));
    free(expected);
}

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    CHECK(!lom_native_map_ps1_ram());
    CHECK(!lom_native_cd_set_disc_root(argv[1]));
    cdrom_init();
    callback_cases(argv[1]);
    chunk_cases();
    malformed_cases(argv[1]);
    cdrom_stop();
    puts("CD callbacks: 2048-byte sectors, NULL backpressure, FIFO/duplicates/capacity, recursive pump, cancel/reset, final word padding and RAM bounds "
         "passed; 14 retail chunk/direct comparisons and malformed stream checks passed.");
    return 0;
}
