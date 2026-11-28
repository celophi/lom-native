#include "../../src/cd/disc_files_provider.h"

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
 * @brief Check the disk's provider reads a whole disc file byte for byte, says it's there and says how big it is.
 * @note Given a file on the disk, as an extracted disc's are, when the provider in use, the disk's, reads it, then it
 *       gets every byte and its size, the file exists and its size is the same.
 */
static void reads_a_whole_file(void)
{
    const unsigned char bytes[] = {'P', 'S', 'B', 0, 1, 2, 3, 0xFF, 0x80, 0x7F};
    const LomDiscFilesProvider* provider = lom_disc_files_provider();
    char path[512];
    snprintf(path, sizeof(path), "%s/lom-disc-files-provider-%d.bin", temporary_folder(), rand());
    FILE* file = fopen(path, "wb");
    CHECK(file && fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes) && fclose(file) == 0);

    size_t size = 0;
    unsigned char* data = provider->read(provider->context, path, &size);
    CHECK(data && size == sizeof(bytes) && memcmp(data, bytes, size) == 0);
    free(data);
    CHECK(provider->exists(provider->context, path));
    CHECK(provider->size(provider->context, path) == (long)sizeof(bytes));

    remove(path);
}

/**
 * @brief Check the disk's provider says a missing file isn't there, and reads and measures nothing from it.
 * @note Given a path with nothing at it, when the provider reads it, checks it and measures it, then it gets NULL, the
 *       path isn't a file and its size is -1.
 */
static void finds_nothing_where_there_is_nothing(void)
{
    const LomDiscFilesProvider* provider = lom_disc_files_provider();
    char path[512];
    snprintf(path, sizeof(path), "%s/lom-disc-files-provider-missing-%d", temporary_folder(), rand());
    size_t size = 0;

    CHECK(provider->read(provider->context, path, &size) == NULL);
    CHECK(!provider->exists(provider->context, path));
    CHECK(provider->size(provider->context, path) == -1);
}

/**
 * @brief Check a provider put in its place is the one in use, and NULL goes back to the disk's.
 * @note Given another provider is put in use, then it's the one handed out; when NULL is put in use, then the disk's
 *       is again.
 */
static void another_provider_can_stand_in_and_step_back(void)
{
    const LomDiscFilesProvider* disk = lom_disc_files_provider();
    const LomDiscFilesProvider stand_in = {NULL, NULL, NULL, NULL};

    lom_disc_use_files_provider(&stand_in);
    CHECK(lom_disc_files_provider() == &stand_in);
    lom_disc_use_files_provider(NULL);
    CHECK(lom_disc_files_provider() == disk);
}

int main(void)
{
    reads_a_whole_file();
    finds_nothing_where_there_is_nothing();
    another_provider_can_stand_in_and_step_back();
    puts("disc-files-provider: reads the disc's files from the disk");
    return 0;
}
