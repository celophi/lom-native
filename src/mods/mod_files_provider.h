#ifndef LOM_MOD_FILES_PROVIDER_H
#define LOM_MOD_FILES_PROVIDER_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lom::mods
{

class ModFilesProvider
{
  public:
    /**
     * @brief Destroy it.
     */
    virtual ~ModFilesProvider() = default;

    /**
     * @brief Read a whole file of a mod's.
     * @param path The file.
     * @param bytes Set to what's in it.
     * @return True when it could be read.
     */
    virtual bool read(const std::filesystem::path& path, std::string& bytes) = 0;

    /**
     * @brief How big a file of a mod's is.
     * @param path The file.
     * @return Its size, or nothing when it isn't there.
     */
    virtual std::optional<std::uintmax_t> size(const std::filesystem::path& path) = 0;

    /**
     * @brief Whether a file of a mod's is there.
     * @param path The file.
     * @return Whether it is.
     */
    virtual bool is_file(const std::filesystem::path& path) = 0;

    /**
     * @brief Whether a folder is there, like a mod's.
     * @param path The folder.
     * @return Whether it is.
     */
    virtual bool is_folder(const std::filesystem::path& path) = 0;

    /**
     * @brief Every file in a mod's folder and the folders in it.
     * @param folder The folder.
     * @param files Set to the files' paths, relative to the folder.
     * @return True when the folder could be listed.
     */
    virtual bool files_under(const std::filesystem::path& folder, std::vector<std::filesystem::path>& files) = 0;
};

/**
 * @brief The provider in use for mods' files: the disk's, unless the launcher or a test put in its own.
 * @return The provider.
 * @note This is the mods code's provider: the one place it reaches a mod's files and the mods folder, as thin as it can
 *       be. Reading mod.json, mods.ini and scripts, and checking them, are the port's own code. The launcher hands it its own files provider, so its tests
 *       serve mods from memory.
 */
ModFilesProvider& mod_files_provider();

/**
 * @brief Use another provider.
 * @param provider The provider, or nullptr to go back to the disk's. It has to stay valid while it's in use.
 */
void use_mod_files_provider(ModFilesProvider* provider);

}

#endif
