#ifndef LOM_LAUNCHER_UPDATE_PACKAGE_H
#define LOM_LAUNCHER_UPDATE_PACKAGE_H

#include <filesystem>
#include <optional>
#include <string>

namespace lom::launcher {
class UpdatePackage {
  public:
    enum class Kind { AppImage, WindowsFolder, MacBundle };

    static constexpr char checksums_name[] = "SHA256SUMS";
    static constexpr char leftover_suffix[] = ".old-update";

    /**
     * @brief Name the release file a kind of install updates from, as packaging/ builds it.
     * @param kind The kind of install.
     * @param version The release's version, like "1.0.1".
     * @return Like "lom-native-1.0.1-linux-x86_64.AppImage".
     */
    static std::string asset_name(Kind kind, const std::string& version);

    /**
     * @brief Find a file's SHA-256 in a SHA256SUMS file, as sha256sum writes it.
     * @param sums The SHA256SUMS text.
     * @param file_name The file.
     * @return Its digest in lowercase hex, or nothing when it isn't listed.
     */
    static std::optional<std::string> checksum(const std::string& sums, const std::string& file_name);

    /**
     * @brief Put a new file in place of an old one, as an AppImage update does.
     *
     * The old file is kept beside it, with leftover_suffix, until remove_leftovers.
     * @param target The file to replace.
     * @param replacement The new file, on the same file system.
     * @param error Set to why it failed.
     * @return True when the new file is in place; on failure the old one is back.
     */
    static bool replace_file(const std::filesystem::path& target, const std::filesystem::path& replacement, std::string* error);

    /**
     * @brief Put every file of a new folder in place in an installed one, as a Windows update does.
     *
     * Windows can't overwrite a running program but can rename it, so each file
     * that gets replaced is renamed with leftover_suffix first. Files only the old
     * folder has stay.
     * @param target The installed folder.
     * @param replacement The new folder, on the same file system.
     * @param error Set to why it failed.
     * @return True when every new file is in place; on failure the old files are back.
     */
    static bool replace_folder(const std::filesystem::path& target, const std::filesystem::path& replacement, std::string* error);

    /**
     * @brief Put a new folder in place of an old one, as a Mac app bundle update does.
     *
     * The old folder is kept beside it, with leftover_suffix, until remove_leftovers.
     * @param target The folder to replace.
     * @param replacement The new folder, on the same file system.
     * @param error Set to why it failed.
     * @return True when the new folder is in place; on failure the old one is back.
     */
    static bool replace_bundle(const std::filesystem::path& target, const std::filesystem::path& replacement, std::string* error);

    /**
     * @brief Delete what an update left: the old file or folder beside target,
     *        and old files inside it. Ones still in use are left for next time.
     * @param target The file or folder an update replaced.
     */
    static void remove_leftovers(const std::filesystem::path& target);
};

}

#endif
