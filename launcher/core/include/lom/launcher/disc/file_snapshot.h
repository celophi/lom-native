#ifndef LOM_LAUNCHER_FILE_SNAPSHOT_H
#define LOM_LAUNCHER_FILE_SNAPSHOT_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lom::launcher {

class FileManifest;
class Json;

class FileSnapshot {
  public:
    struct Entry {
        std::string path;
        std::uint64_t bytes = 0;
        std::int64_t modified_ticks = 0;
    };

    static constexpr int schema_version = 1;

    /**
     * @brief Record the current lengths and times of a verified folder's files.
     * @param root Folder containing the files under their canonical names.
     * @param manifest Manifest the folder was verified against.
     * @return The snapshot, or nothing when a file cannot be examined.
     */
    static std::optional<FileSnapshot> capture(const std::filesystem::path& root, const FileManifest& manifest);

    /**
     * @brief Read a snapshot file.
     * @param path Snapshot file.
     * @return The snapshot, or nothing when absent or malformed.
     */
    static std::optional<FileSnapshot> load(const std::filesystem::path& path);

    /**
     * @brief Write the snapshot atomically.
     * @param path Snapshot file.
     * @param error Receives a description on failure; may be null.
     * @return True when written.
     */
    bool save(const std::filesystem::path& path, std::string* error) const;

    /**
     * @brief Check that the snapshot describes a folder verified against a manifest.
     * @param folder Folder.
     * @param manifest_identity Aggregate identity of the manifest.
     * @return True when both match.
     */
    bool applies_to(const std::filesystem::path& folder, const std::string& manifest_identity) const;

    /**
     * @brief Check whether a file still has its verified length and time.
     * @param path Canonical relative path.
     * @param bytes Current length.
     * @param modified_ticks Current modification time.
     * @return True when the file is unchanged since verification.
     */
    bool unchanged(const std::string& path, std::uint64_t bytes, std::int64_t modified_ticks) const;

    std::filesystem::path root;
    std::string manifest_sha256;
    std::vector<Entry> entries;
};

}

#endif
