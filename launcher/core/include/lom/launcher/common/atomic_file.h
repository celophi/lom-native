#ifndef LOM_LAUNCHER_ATOMIC_FILE_H
#define LOM_LAUNCHER_ATOMIC_FILE_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lom::launcher {
class AtomicFile {
  public:
    static constexpr std::uintmax_t max_read_bytes = 64u * 1024u * 1024u;

    /**
     * @brief Write a file by writing a sibling temporary and renaming it over the target.
     * @param path Destination; its directory is created if needed.
     * @param contents Bytes to write.
     * @param error Receives a description on failure; may be null.
     * @return True when the complete contents replaced the target.
     */
    static bool write(const std::filesystem::path& path, const std::string& contents, std::string* error);

    /**
     * @brief Read a whole file.
     * @param path File to read.
     * @param error Receives a description on failure; may be null.
     * @return The contents, or nothing if the file is missing, unreadable or larger than max_read_bytes.
     */
    static std::optional<std::string> read(const std::filesystem::path& path, std::string* error);

    /**
     * @brief Read a whole file as bytes.
     * @param path File to read.
     * @param bytes Receives the contents; left alone on failure.
     * @return True when it was read; false if it's missing, unreadable or larger than max_read_bytes.
     */
    static bool read_bytes(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes);
};

}

#endif
