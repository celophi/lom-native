#include "../../src/remaster/remaster_files_provider.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../support/lom_check.h"

/**
 * @brief The folder for the test's file: the system's temporary folder, or the current one.
 * @return The folder.
 */
static const char* temporary_folder(void)
{
    const char* const variables[] = {"TMPDIR", "TEMP", "TMP"};

    for (unsigned i = 0; i < sizeof(variables) / sizeof(variables[0]); ++i)
    {
        const char* folder = getenv(variables[i]);

        if (folder && *folder)
        {
            return folder;
        }
    }

    return ".";
}

/**
 * @brief Check the disk's provider reads a whole file byte for byte, says it's there and not a folder, and says a
 *        folder is a folder.
 * @note Given a file on the disk, when the provider in use, the disk's, reads it, then it gets every byte and its
 *       size, the file exists and isn't a folder, and its folder is one.
 */
static void reads_a_file_and_knows_files_from_folders(void)
{
    const unsigned char bytes[] = {'P', 'S', 'B', 0, 1, 2, 3, 0xFF, 0x80, 0x7F};
    const LomRemasterFilesProvider* provider = lom_remaster_files_provider();
    char path[512];
    snprintf(path, sizeof(path), "%s/lom-remaster-files-provider-%d.bin", temporary_folder(), rand());
    FILE* file = fopen(path, "wb");
    CHECK(file && fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes) && fclose(file) == 0);

    size_t size = 0;
    unsigned char* data = provider->read(provider->context, path, &size);
    CHECK(data && size == sizeof(bytes) && memcmp(data, bytes, size) == 0);
    free(data);
    CHECK(provider->exists(provider->context, path));
    CHECK(!provider->is_folder(provider->context, path));
    CHECK(provider->is_folder(provider->context, temporary_folder()));

    remove(path);
}

/**
 * @brief Check the disk's provider says a missing file and folder aren't there, and reads nothing from them.
 * @note Given a path with nothing at it, when the provider reads it and checks it, then it gets NULL, and the path
 *       is neither a file nor a folder.
 */
static void finds_nothing_where_there_is_nothing(void)
{
    const LomRemasterFilesProvider* provider = lom_remaster_files_provider();
    char path[512];
    snprintf(path, sizeof(path), "%s/lom-remaster-files-provider-missing-%d", temporary_folder(), rand());
    size_t size = 0;

    CHECK(provider->read(provider->context, path, &size) == NULL);
    CHECK(!provider->exists(provider->context, path));
    CHECK(!provider->is_folder(provider->context, path));
}

/**
 * @brief Check a provider put in its place is the one in use, and NULL goes back to the disk's.
 * @note Given another provider is put in use, then it's the one handed out; when NULL is put in use, then the disk's
 *       is again.
 */
static void another_provider_can_stand_in_and_step_back(void)
{
    const LomRemasterFilesProvider* disk = lom_remaster_files_provider();
    const LomRemasterFilesProvider stand_in = {NULL, NULL, NULL, NULL};

    lom_remaster_use_files_provider(&stand_in);
    CHECK(lom_remaster_files_provider() == &stand_in);
    lom_remaster_use_files_provider(NULL);
    CHECK(lom_remaster_files_provider() == disk);
}

int main(void)
{
    reads_a_file_and_knows_files_from_folders();
    finds_nothing_where_there_is_nothing();
    another_provider_can_stand_in_and_step_back();
    puts("remaster-files-provider: reads files and tells files and folders apart on the disk");
    return 0;
}
