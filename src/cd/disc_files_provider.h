#ifndef LOM_DISC_FILES_PROVIDER_H
#define LOM_DISC_FILES_PROVIDER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct LomDiscFilesProvider
    {
        unsigned char* (*read)(void* context, const char* path, size_t* size);
        int (*exists)(void* context, const char* path);
        long (*size)(void* context, const char* path);
        void* context;
    } LomDiscFilesProvider;

    /**
     * @brief The provider in use for the PS1 disc's files: the disk's, where
     *        the player's extracted disc is, unless a test put in its own.
     * @return The provider. Its calls each take its @c context first:
     *         @c read(context, path, &size) reads a whole file and returns its
     *         bytes for the caller to free, or NULL when it can't be read;
     *         @c exists(context, path) says whether a file is there and can be
     *         read; @c size(context, path) says how big a file is, or -1 when
     *         it isn't there. Paths are whole, the disc's folder
     *         (lom_native_cd_get_disc_root) and the file's path on the disc.
     * @note This is the disc's provider: the one place the port reaches the
     *       extracted disc's files, kept as thin as it can be. Everything made
     *       from them is the port's own code, which tests run for real with a
     *       provider of their own that serves files from memory
     *       (tests/support/cd/disc_files_provider_fake.h). The CD layer
     *       (src/cd/cdrom_native.c) still reads the disc itself, its sectors as
     *       the game streams them; tools/testing/provider_allow.json lists
     *       those reads, and provider_check.py keeps anything new from going
     *       around this.
     */
    const LomDiscFilesProvider* lom_disc_files_provider(void);

    /**
     * @brief Use another provider, as tests do.
     * @param files The provider, or NULL to go back to the disk's. It has to
     *        stay valid while it's in use.
     */
    void lom_disc_use_files_provider(const LomDiscFilesProvider* files);

#ifdef __cplusplus
}
#endif
#endif
