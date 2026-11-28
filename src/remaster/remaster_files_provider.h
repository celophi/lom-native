#ifndef LOM_REMASTER_FILES_PROVIDER_H
#define LOM_REMASTER_FILES_PROVIDER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct LomRemasterFilesProvider
    {
        unsigned char* (*read)(void* context, const char* path, size_t* size);
        int (*exists)(void* context, const char* path);
        int (*is_folder)(void* context, const char* path);
        void* context;
    } LomRemasterFilesProvider;

    /**
     * @brief The provider in use for the Remaster's files: the disk's, unless a
     *        test put in its own.
     * @return The provider. Its calls each take its @c context first:
     *         @c read(context, path, &size) reads a whole file and returns its
     *         bytes for the caller to free, or NULL when it can't be read;
     *         @c exists(context, path) says whether a file is there and can be
     *         read; @c is_folder(context, path) says whether a folder is there.
     * @note This is the Remaster code's provider: the one place it reaches the
     *       disk for the Remaster's files, kept as thin as it can be.
     *       Everything above it (unpacking .m files, reading PSBs, the
     *       pictures, text and sound made from them) is the port's own code,
     *       which tests run for real with a provider of their own that serves
     *       files from memory (tests/support/platform/remaster_files_provider_fake.h).
     *       tools/testing/provider_check.py keeps the rest of src/remaster/
     *       from going around it.
     */
    const LomRemasterFilesProvider* lom_remaster_files_provider(void);

    /**
     * @brief Use another provider, as tests do.
     * @param files The provider, or NULL to go back to the disk's. It has to
     *        stay valid while it's in use.
     */
    void lom_remaster_use_files_provider(const LomRemasterFilesProvider* files);

#ifdef __cplusplus
}
#endif
#endif
