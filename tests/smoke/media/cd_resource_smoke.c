#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "main/cdrom.h"
#include "main/cd_resources.h"

#include "lom_native_cd.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_fs.h"
#include "../../support/platform/lom_test_posix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_DESTINATION ((void*)MEMORY_MAP_RAM(0x80140000))
#define TEST_RESOURCE CD_RES_TITLE_BIN

/**
 * @brief Check a resource read matches its file on the disc.
 * @param path The file.
 * @param loaded_data What was read.
 * @param size How many bytes.
 * @return 0, or -1 when it doesn't match or the file can't be read.
 */
static int verify_resource_bytes(const char* path, const void* loaded_data, size_t size)
{
    FILE* file;
    unsigned char* expected;
    size_t bytes_read;
    int matches;

    expected = (unsigned char*)malloc(size);

    if (expected == NULL)
    {
        fprintf(stderr, "Unable to allocate %zu-byte comparison buffer.\n", size);
        return -1;
    }

    file = fopen(path, "rb");

    if (file == NULL)
    {
        fprintf(stderr, "Unable to open %s for verification.\n", path);
        free(expected);
        return -1;
    }

    bytes_read = fread(expected, 1, size, file);
    fclose(file);

    matches = bytes_read == size && memcmp(expected, loaded_data, size) == 0;
    free(expected);
    return matches ? 0 : -1;
}

static unsigned size_query_checks, size_query_failures, size_query_callback_calls;

/**
 * @brief A sector callback that holds every sector, to keep a read pending.
 * @param transferred Ignored.
 * @param remaining Ignored.
 * @return NULL.
 */
static u8* size_query_pending_callback(s32 transferred, u32 remaining)
{
    (void)transferred;
    (void)remaining;
    ++size_query_callback_calls;
    return NULL;
}

#define SIZE_QUERY_CHECK(condition)                                                                                                                            \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        ++size_query_checks;                                                                                                                                   \
        if (!(condition))                                                                                                                                      \
        {                                                                                                                                                      \
            ++size_query_failures;                                                                                                                             \
            if (size_query_failures < 16)                                                                                                                      \
                fprintf(stderr, "Size query %d at line %d: %s\n", (int)query, __LINE__, #condition);                                                           \
        }                                                                                                                                                      \
    } while (0)

/**
 * @brief Check asking a resource's size reads only its low 16 bits, refuses IDs
 *        out of range, and doesn't wait for or disturb a read that's pending.
 * @return 0, or -1 when a check fails.
 */
static int verify_size_query_contract(void)
{
    unsigned char original_sizes[5811 * 8];
    static const s32 highwords[] = {0, 0x00010000, 0x12340000, 0x7fff0000, (-2147483647 - 1), -65536};
    static const s32 strict_ids[] = {65540, 0x12340004, -2147483644, -65532};
    char path[4096], original_root[4096];
    char private_root[4096];
    char bin_path[4096] = {0}, title_path[4096] = {0};
    int root_changed = 0, private_created = 0, bin_created = 0, file_created = 0;
    s32 query = TEST_RESOURCE;
    unsigned epoch = lom_native_cd_reset_epoch();
    int length = snprintf(original_root, sizeof(original_root), "%s", lom_native_cd_get_disc_root());

    if (length < 0 || (size_t)length >= sizeof(original_root))
    {
        return -1;
    }

    length = snprintf(path, sizeof(path), "%s/SKCDPOSE.DAT", original_root);

    if (length < 0 || (size_t)length >= sizeof(path))
    {
        return -1;
    }

    {
        FILE* table = fopen(path, "rb");

        if (!table)
        {
            SIZE_QUERY_CHECK(0);
            return -1;
        }

        size_t got = fread(original_sizes, 1, sizeof(original_sizes), table);
        int tail = fgetc(table), read_error = ferror(table), closed = fclose(table);
        SIZE_QUERY_CHECK(got == sizeof(original_sizes) && tail == EOF && !read_error && !closed);

        if (got != sizeof(original_sizes) || tail != EOF || read_error || closed)
        {
            return -1;
        }

        SIZE_QUERY_CHECK(original_sizes[4] == 68 && original_sizes[5] == 0 && original_sizes[6] == 0 && original_sizes[7] == 0);
        SIZE_QUERY_CHECK(original_sizes[4 * 8 + 4] == 0xd5 && original_sizes[4 * 8 + 5] == 0x49 && original_sizes[4 * 8 + 6] == 4 &&
                         original_sizes[4 * 8 + 7] == 0);
        SIZE_QUERY_CHECK(original_sizes[19 * 8 + 4] == 0 && original_sizes[19 * 8 + 5] == 0 && original_sizes[19 * 8 + 6] == 0 &&
                         original_sizes[19 * 8 + 7] == 0);
    }

    SIZE_QUERY_CHECK(cdrom_queue_read_with_callback(TEST_RESOURCE, size_query_pending_callback) == 281045);
    SIZE_QUERY_CHECK(lom_native_cd_callback_pending(TEST_RESOURCE, size_query_pending_callback) == 1);

    for (unsigned error_case = 0; error_case < 2; ++error_case)
    {
        if (error_case)
        {
            SIZE_QUERY_CHECK(lom_native_cd_resolve_resource_path(65535, path, sizeof(path)) == -1);
        }

        LomNativeCdError expected_error = error_case ? LOM_NATIVE_CD_ERROR_INVALID_RESOURCE : LOM_NATIVE_CD_ERROR_NONE;

        for (unsigned r = 0; r < 5811 + 2; ++r)
        {
            s32 id = r < 5811 ? (s32)r : (r == 5811 ? 5811 : 65535);
            u32 size = 0;

            if (r < 5811)
            {
                const unsigned char* word = original_sizes + r * 8 + 4;
                size = (u32)word[0] | ((u32)word[1] << 8) | ((u32)word[2] << 16) | ((u32)word[3] << 24);
                SIZE_QUERY_CHECK(size <= 0x7fffffffu);
            }

            for (unsigned h = 0; h < sizeof(highwords) / sizeof(highwords[0]); ++h)
            {
                query = highwords[h] + id;
                SIZE_QUERY_CHECK(cdrom_get_resource_size(query) == (s32)size);
                SIZE_QUERY_CHECK(lom_native_cd_get_last_error() == expected_error);
                SIZE_QUERY_CHECK(cdrom_get_error_status() == (error_case ? 1 : 0));
                SIZE_QUERY_CHECK(lom_native_cd_reset_epoch() == epoch);
                SIZE_QUERY_CHECK(lom_native_cd_callback_pending(TEST_RESOURCE, size_query_pending_callback) == 1);
                SIZE_QUERY_CHECK(cdrom_can_queue_resource(TEST_RESOURCE) == 0);
                SIZE_QUERY_CHECK(size_query_callback_calls == 0);
            }
        }
    }

    for (unsigned i = 0; i < sizeof(strict_ids) / sizeof(strict_ids[0]); ++i)
    {
        query = strict_ids[i];
        memset(path, 'Z', sizeof(path));
        SIZE_QUERY_CHECK(lom_native_cd_resolve_resource_path(query, path, sizeof(path)) == -1);
        SIZE_QUERY_CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_INVALID_RESOURCE);
        int path_unchanged = 1;

        for (unsigned j = 0; j < sizeof(path); ++j)
        {
            if (path[j] != 'Z')
            {
                path_unchanged = 0;
            }
        }

        SIZE_QUERY_CHECK(path_unchanged);
    }

    SIZE_QUERY_CHECK(size_query_callback_calls == 0);
    SIZE_QUERY_CHECK(lom_native_cd_cancel_callback(TEST_RESOURCE, size_query_pending_callback) == 1);
    SIZE_QUERY_CHECK(!lom_native_cd_callback_pending(TEST_RESOURCE, size_query_pending_callback));

    if (!lom_test_make_temp_folder(private_root, sizeof(private_root), "lom-cd-size"))
    {
        SIZE_QUERY_CHECK(0);
        goto cleanup;
    }

    private_created = 1;
    length = snprintf(bin_path, sizeof(bin_path), "%s/BIN", private_root);

    if (length < 0 || (size_t)length >= sizeof(bin_path))
    {
        SIZE_QUERY_CHECK(0);
        goto cleanup;
    }

    length = snprintf(title_path, sizeof(title_path), "%s/TITLE.BIN", bin_path);

    if (length < 0 || (size_t)length >= sizeof(title_path))
    {
        SIZE_QUERY_CHECK(0);
        goto cleanup;
    }

    if (lom_native_make_folder(bin_path) != 0)
    {
        SIZE_QUERY_CHECK(0);
        goto cleanup;
    }

    bin_created = 1;

    if (lom_native_cd_set_disc_root(private_root) != 0)
    {
        SIZE_QUERY_CHECK(0);
        goto cleanup;
    }

    root_changed = 1;
    query = TEST_RESOURCE;
    SIZE_QUERY_CHECK(cdrom_queue_read(query, TEST_DESTINATION) == -2);
    SIZE_QUERY_CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_NOT_FOUND);

    for (unsigned h = 0; h < sizeof(highwords) / sizeof(highwords[0]); ++h)
    {
        query = highwords[h] + TEST_RESOURCE;
        SIZE_QUERY_CHECK(cdrom_get_resource_size(query) == 281045);
        SIZE_QUERY_CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_NOT_FOUND);
        SIZE_QUERY_CHECK(cdrom_get_error_status() == 1);
    }

    {
        FILE* file = fopen(title_path, "wb");

        if (!file)
        {
            SIZE_QUERY_CHECK(0);
            goto cleanup;
        }

        file_created = 1;
        int written = fputc(0x7b, file), closed = fclose(file);

        if (written != 0x7b || closed != 0)
        {
            SIZE_QUERY_CHECK(0);
            goto cleanup;
        }
    }

    query = TEST_RESOURCE;
    SIZE_QUERY_CHECK(cdrom_queue_read(query, TEST_DESTINATION) == 1);
    SIZE_QUERY_CHECK(*(const unsigned char*)TEST_DESTINATION == 0x7b);
    SIZE_QUERY_CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_NONE);

    for (unsigned h = 0; h < sizeof(highwords) / sizeof(highwords[0]); ++h)
    {
        query = highwords[h] + TEST_RESOURCE;
        SIZE_QUERY_CHECK(cdrom_get_resource_size(query) == 281045);
        SIZE_QUERY_CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_NONE);
        SIZE_QUERY_CHECK(cdrom_get_error_status() == 0);
    }

cleanup:

    if (root_changed)
    {
        SIZE_QUERY_CHECK(lom_native_cd_set_disc_root(original_root) == 0);
    }

    if (file_created)
    {
        SIZE_QUERY_CHECK(unlink(title_path) == 0);
    }

    if (bin_created)
    {
        SIZE_QUERY_CHECK(rmdir(bin_path) == 0);
    }

    if (private_created)
    {
        SIZE_QUERY_CHECK(rmdir(private_root) == 0);
    }

    cdrom_reset();
    printf("  size queries: %u checks, %u failures\n", size_query_checks, size_query_failures);
    return size_query_failures ? -1 : 0;
}

#undef SIZE_QUERY_CHECK

int main(int argc, char** argv)
{
    char resource_path[4096];
    const char* disc_root;
    s32 resource_size;

    if (lom_native_map_ps1_ram() != 0)
    {
        return 1;
    }

    if (argc > 2)
    {
        fprintf(stderr, "usage: %s [extracted-disc-root]\n", argv[0]);
        return 2;
    }

    if (argc == 2 && lom_native_cd_set_disc_root(argv[1]) != 0)
    {
        fprintf(stderr, "Invalid extracted-disc root: %s\n", argv[1]);
        return 2;
    }

    cdrom_init();
    cdrom_load_resource_table(0x18, 0xB598);

    if (verify_size_query_contract() != 0)
    {
        return 1;
    }

    disc_root = lom_native_cd_get_disc_root();

    if (lom_native_cd_resolve_resource_path(TEST_RESOURCE, resource_path, sizeof(resource_path)) != 0)
    {
        fprintf(stderr, "TITLE.BIN is not mapped by the native resource backend.\n");
        return 1;
    }

    resource_size = cdrom_get_resource_size(TEST_RESOURCE);

    if (resource_size <= 0)
    {
        fprintf(stderr, "Unable to find %s below disc root '%s'.\n", resource_path, disc_root);
        return 1;
    }

    cdrom_queue_read(TEST_RESOURCE, TEST_DESTINATION);
    cdrom_wait_queue_empty();

    if (cdrom_get_error_status() != 0)
    {
        fprintf(stderr, "Native CD read failed for %s (error %d).\n", resource_path, (int)lom_native_cd_get_last_error());
        return 1;
    }

    if (verify_resource_bytes(resource_path, TEST_DESTINATION, (size_t)resource_size) != 0)
    {
        fprintf(stderr, "Loaded resource bytes differ from %s.\n", resource_path);
        return 1;
    }

    {
        static const struct
        {
            int id;
            const char* path;
        } text_assets[] = {
            {0xB1, "MAP/FDATA/M_WIN.PRS"},
            {0xB2, "MAP/FDATA/FONT.PRS"},
        };

        size_t i;

        for (i = 0; i < sizeof(text_assets) / sizeof(text_assets[0]); i++)
        {
            char expected_path[4096];
            s32 size = cdrom_get_resource_size(text_assets[i].id);
            int length = snprintf(expected_path, sizeof(expected_path), "%s/%s", disc_root, text_assets[i].path);

            if (length < 0 || (size_t)length >= sizeof(expected_path) || size <= 0)
            {
                fprintf(stderr, "Missing text resource 0x%X.\n", text_assets[i].id);
                return 1;
            }

            cdrom_queue_read(text_assets[i].id, TEST_DESTINATION);
            cdrom_wait_queue_empty();

            if (cdrom_get_error_status() != 0 || verify_resource_bytes(expected_path, TEST_DESTINATION, (size_t)size) != 0)
            {
                fprintf(stderr, "Text resource 0x%X does not match %s.\n", text_assets[i].id, expected_path);
                return 1;
            }
        }
    }

    printf("LoM CD resource smoke passed.\n");
    printf("  disc root: %s\n", disc_root);
    printf("  resource:  %d (%s)\n", TEST_RESOURCE, resource_path);
    printf("  bytes:     %d\n", resource_size);
    printf("  address:   %p\n", TEST_DESTINATION);
    return 0;
}
