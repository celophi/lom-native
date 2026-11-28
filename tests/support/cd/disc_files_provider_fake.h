#ifndef LOM_DISC_FILES_PROVIDER_FAKE_H
#define LOM_DISC_FILES_PROVIDER_FAKE_H

#include "../../../src/cd/disc_files_provider.h"

#include <stdlib.h>
#include <string.h>

#define DISC_FILES_PROVIDER_FAKE_FILES 32
#define DISC_FILES_PROVIDER_FAKE_PATH 256

typedef struct
{
    char path[DISC_FILES_PROVIDER_FAKE_PATH];
    const unsigned char* bytes;
    size_t size;
    unsigned reads;
} DiscFilesProviderFakeFile;

typedef struct
{
    DiscFilesProviderFakeFile files[DISC_FILES_PROVIDER_FAKE_FILES];
    unsigned file_count;
    unsigned misses;
    LomDiscFilesProvider provider;
} DiscFilesProviderFake;

/**
 * @brief Find one of the fake's files.
 * @param fake The fake.
 * @param path Its path.
 * @return The file, or NULL when the fake doesn't have it.
 */
static inline DiscFilesProviderFakeFile* disc_files_provider_fake_find(DiscFilesProviderFake* fake, const char* path)
{
    for (unsigned i = 0; i < fake->file_count; ++i)
    {
        if (strcmp(fake->files[i].path, path) == 0)
        {
            return &fake->files[i];
        }
    }

    return NULL;
}

/**
 * @brief The fake's read: a copy of the file's bytes, counting the read, or NULL (and a miss) when it hasn't got it.
 * @param context The fake.
 * @param path The file.
 * @param size Set to its size.
 * @return The copy, which the caller frees, or NULL.
 */
static inline unsigned char* disc_files_provider_fake_read(void* context, const char* path, size_t* size)
{
    DiscFilesProviderFake* fake = context;
    DiscFilesProviderFakeFile* file = disc_files_provider_fake_find(fake, path);
    unsigned char* copy = file ? malloc(file->size ? file->size : 1) : NULL;

    if (!copy)
    {
        fake->misses++;
        return NULL;
    }

    memcpy(copy, file->bytes, file->size);
    *size = file->size;
    file->reads++;
    return copy;
}

/**
 * @brief The fake's file check: whether it has the file.
 * @param context The fake.
 * @param path The file.
 * @return Nonzero when it has it.
 */
static inline int disc_files_provider_fake_exists(void* context, const char* path)
{
    return disc_files_provider_fake_find(context, path) != NULL;
}

/**
 * @brief The fake's size: how big the file it was given is.
 * @param context The fake.
 * @param path The file.
 * @return Its size, or -1 when the fake hasn't got it.
 */
static inline long disc_files_provider_fake_size(void* context, const char* path)
{
    DiscFilesProviderFakeFile* file = disc_files_provider_fake_find(context, path);
    return file ? (long)file->size : -1;
}

/**
 * @brief Start a fake provider of the disc's files with nothing in it, and put it in use in place of the disk's,
 *        so the code under test reads only what the test gives it.
 * @param fake The fake, which has to stay valid until disc_files_provider_fake_stop.
 */
static inline void disc_files_provider_fake_start(DiscFilesProviderFake* fake)
{
    memset(fake, 0, sizeof(*fake));
    fake->provider.read = disc_files_provider_fake_read;
    fake->provider.exists = disc_files_provider_fake_exists;
    fake->provider.size = disc_files_provider_fake_size;
    fake->provider.context = fake;
    lom_disc_use_files_provider(&fake->provider);
}

/**
 * @brief Give the fake a file. Its bytes aren't copied, so they have to stay valid while it's in use.
 * @param fake The fake.
 * @param path The file's path.
 * @param bytes Its bytes.
 * @param size How many.
 * @return The file, to check how often it was read.
 */
static inline DiscFilesProviderFakeFile* disc_files_provider_fake_add(DiscFilesProviderFake* fake, const char* path, const unsigned char* bytes,
                                                                              size_t size)
{
    DiscFilesProviderFakeFile* file = &fake->files[fake->file_count++];
    strncpy(file->path, path, sizeof(file->path) - 1);
    file->bytes = bytes;
    file->size = size;
    return file;
}

/**
 * @brief Go back to the disk's provider.
 */
static inline void disc_files_provider_fake_stop(void)
{
    lom_disc_use_files_provider(NULL);
}

#endif
