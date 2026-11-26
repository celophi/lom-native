#ifndef LOM_NATIVE_MOD_DISC_H
#define LOM_NATIVE_MOD_DISC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Read a file from the player's disc, as it is on the disc. Nothing
     *        outside the disc can be read.
     * @param path Its path on the disc, like "ANA/MAPINFO/BON_1F00.IMG", with
     *        forward slashes; no "." or ".." parts.
     * @param size Set to its size.
     * @return Its bytes, to free, or NULL when there's no such file or the
     *         path isn't allowed.
     */
    unsigned char* lom_native_mod_disc_read(const char* path, size_t* size);

    /**
     * @brief List the files in a folder on the player's disc and every folder
     *        under it, sorted by path.
     * @param folder The folder, like "ANA/MAPINFO", or "" for the whole disc.
     * @param found Called with each file's path on the disc.
     * @param context Passed to @p found.
     * @return 0, or -1 when there's no such folder or the path isn't allowed.
     */
    int lom_native_mod_disc_list(const char* folder, void (*found)(const char* path, void* context), void* context);

    /**
     * @brief Unpack a file the game keeps compressed on the disc, the way the
     *        game does when it streams one.
     * @param data The file, with its one-byte header.
     * @param size Its size.
     * @param unpacked_size Set to the unpacked size.
     * @return The unpacked bytes, to free, or NULL when it isn't a compressed
     *         file.
     */
    unsigned char* lom_native_mod_disc_unpack(const unsigned char* data, size_t size, size_t* unpacked_size);

#ifdef __cplusplus
}
#endif
#endif
