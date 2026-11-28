#include "disc_files_provider.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

/**
 * @brief Read a whole file from the disk.
 * @param context Unused.
 * @param path The file.
 * @param size Set to its size.
 * @return Its bytes, which the caller frees, or NULL.
 */
static unsigned char* disk_read(void* context, const char* path, size_t* size)
{
    (void)context;
    FILE* file = fopen(path, "rb");

    if (!file)
    {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    const long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char* data = length > 0 ? malloc((size_t)length) : NULL;

    if (!data || fread(data, 1, (size_t)length, file) != (size_t)length)
    {
        free(data);
        fclose(file);
        return NULL;
    }

    fclose(file);
    *size = (size_t)length;
    return data;
}

/**
 * @brief Check a file on the disk can be opened.
 * @param context Unused.
 * @param path The file.
 * @return Nonzero when it can.
 */
static int disk_exists(void* context, const char* path)
{
    (void)context;
    FILE* file = fopen(path, "rb");

    if (!file)
    {
        return 0;
    }

    fclose(file);
    return 1;
}

/**
 * @brief Measure a file on the disk.
 * @param context Unused.
 * @param path The file.
 * @return Its size, or -1 when it isn't a file there.
 */
static long disk_size(void* context, const char* path)
{
    (void)context;
    struct stat st;
    return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG ? (long)st.st_size : -1;
}

static const LomDiscFilesProvider s_disk = {disk_read, disk_exists, disk_size, NULL};
static const LomDiscFilesProvider* s_files = &s_disk;

const LomDiscFilesProvider* lom_disc_files_provider(void)
{
    return s_files;
}

void lom_disc_use_files_provider(const LomDiscFilesProvider* files)
{
    s_files = files ? files : &s_disk;
}
