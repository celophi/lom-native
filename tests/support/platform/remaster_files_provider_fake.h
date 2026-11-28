#ifndef LOM_REMASTER_FILES_PROVIDER_FAKE_H
#define LOM_REMASTER_FILES_PROVIDER_FAKE_H

#include "../../../src/remaster/remaster_files_provider.h"

#include <stdlib.h>
#include <string.h>

#define REMASTER_FILES_PROVIDER_FAKE_FILES 32
#define REMASTER_FILES_PROVIDER_FAKE_FOLDERS 8
#define REMASTER_FILES_PROVIDER_FAKE_PATH 256

typedef struct
{
    char path[REMASTER_FILES_PROVIDER_FAKE_PATH];
    const unsigned char* bytes;
    size_t size;
    unsigned reads;
} RemasterFilesProviderFakeFile;

typedef struct
{
    RemasterFilesProviderFakeFile files[REMASTER_FILES_PROVIDER_FAKE_FILES];
    unsigned file_count;
    char folders[REMASTER_FILES_PROVIDER_FAKE_FOLDERS][REMASTER_FILES_PROVIDER_FAKE_PATH];
    unsigned folder_count;
    unsigned misses;
    LomRemasterFilesProvider provider;
} RemasterFilesProviderFake;

/**
 * @brief Find one of the fake's files.
 * @param fake The fake.
 * @param path Its path.
 * @return The file, or NULL when the fake doesn't have it.
 */
static inline RemasterFilesProviderFakeFile* remaster_files_provider_fake_find(RemasterFilesProviderFake* fake, const char* path)
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
static inline unsigned char* remaster_files_provider_fake_read(void* context, const char* path, size_t* size)
{
    RemasterFilesProviderFake* fake = context;
    RemasterFilesProviderFakeFile* file = remaster_files_provider_fake_find(fake, path);
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
static inline int remaster_files_provider_fake_exists(void* context, const char* path)
{
    return remaster_files_provider_fake_find(context, path) != NULL;
}

/**
 * @brief The fake's folder check: whether it was given the folder.
 * @param context The fake.
 * @param path The folder.
 * @return Nonzero when it was.
 */
static inline int remaster_files_provider_fake_is_folder(void* context, const char* path)
{
    RemasterFilesProviderFake* fake = context;

    for (unsigned i = 0; i < fake->folder_count; ++i)
    {
        if (strcmp(fake->folders[i], path) == 0)
        {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Start a fake provider of the Remaster's files with nothing in it, and put it in use in place of the disk's,
 *        so the code under test reads only what the test gives it.
 * @param fake The fake, which has to stay valid until remaster_files_provider_fake_stop.
 */
static inline void remaster_files_provider_fake_start(RemasterFilesProviderFake* fake)
{
    memset(fake, 0, sizeof(*fake));
    fake->provider.read = remaster_files_provider_fake_read;
    fake->provider.exists = remaster_files_provider_fake_exists;
    fake->provider.is_folder = remaster_files_provider_fake_is_folder;
    fake->provider.context = fake;
    lom_remaster_use_files_provider(&fake->provider);
}

/**
 * @brief Give the fake a file. Its bytes aren't copied, so they have to stay valid while it's in use.
 * @param fake The fake.
 * @param path The file's path.
 * @param bytes Its bytes.
 * @param size How many.
 * @return The file, to check how often it was read.
 */
static inline RemasterFilesProviderFakeFile* remaster_files_provider_fake_add(RemasterFilesProviderFake* fake, const char* path, const unsigned char* bytes,
                                                                              size_t size)
{
    RemasterFilesProviderFakeFile* file = &fake->files[fake->file_count++];
    strncpy(file->path, path, sizeof(file->path) - 1);
    file->bytes = bytes;
    file->size = size;
    return file;
}

/**
 * @brief Give the fake a folder.
 * @param fake The fake.
 * @param path The folder's path.
 */
static inline void remaster_files_provider_fake_add_folder(RemasterFilesProviderFake* fake, const char* path)
{
    strncpy(fake->folders[fake->folder_count++], path, REMASTER_FILES_PROVIDER_FAKE_PATH - 1);
}

/**
 * @brief Go back to the disk's provider.
 */
static inline void remaster_files_provider_fake_stop(void)
{
    lom_remaster_use_files_provider(NULL);
}

#endif
