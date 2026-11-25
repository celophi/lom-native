#ifndef LOM_LAUNCHER_FILE_MANIFEST_H
#define LOM_LAUNCHER_FILE_MANIFEST_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lom::launcher {

class Json;

class FileManifest {
  public:
    struct Entry {
        std::string path;
        std::uint64_t bytes = 0;
        std::string sha256;
    };

    static constexpr int schema_version = 1;

    /**
     * @brief Load a manifest file.
     * @param path Manifest JSON.
     * @param error Receives a description on failure; may be null.
     * @return True when the manifest is complete and well formed.
     */
    bool load(const std::filesystem::path& path, std::string* error);

    /**
     * @brief Load a manifest from parsed JSON.
     * @param document Parsed manifest.
     * @param error Receives a description on failure; may be null.
     * @return True when the manifest is complete and well formed.
     */
    bool from_json(const Json& document, std::string* error);

    /**
     * @brief Serialize the manifest.
     * @return Manifest JSON, entries sorted by path.
     */
    Json to_json() const;

    /**
     * @brief Compute the aggregate tree identity.
     * @return SHA-256 over sorted "path\\0bytes\\0sha256\\n" records.
     */
    std::string aggregate_sha256() const;

    /**
     * @brief Sort entries by path; required after editing entries directly and before find().
     */
    void sort_entries();

    /**
     * @brief Find an entry by canonical path.
     * @param path Canonical relative path.
     * @return The entry, or null. Entries must be sorted.
     */
    const Entry* find(const std::string& path) const;

    /**
     * @brief Sum the expected file sizes.
     * @return Total bytes.
     */
    std::uint64_t total_bytes() const;

    std::string release_id;
    std::string parent_raw_sha1;
    std::string upstream_revision;
    std::string generator;
    std::vector<Entry> entries;
};

}

#endif
