#ifndef LOM_LAUNCHER_TEST_DISK_H
#define LOM_LAUNCHER_TEST_DISK_H

#include "lom/launcher/common/files_provider.h"

#include <filesystem>
#include <istream>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace lom::launcher::test::disk {

/**
 * @brief Make a folder and those it's in, on the disk in use (a test's fake, or the real one).
 * @param path The folder.
 * @return Whether any were made.
 */
inline bool create_directories(const std::filesystem::path& path) {
    std::error_code code;
    return files_provider().create_directories(path, code);
}

/**
 * @brief Whether something is at a path, on the disk in use.
 * @param path The path.
 * @return Whether it is.
 */
inline bool exists(const std::filesystem::path& path) {
    std::error_code code;
    return files_provider().exists(path, code);
}

/**
 * @brief Whether a path is a folder, on the disk in use.
 * @param path The path.
 * @return Whether it is.
 */
inline bool is_directory(const std::filesystem::path& path) {
    std::error_code code;
    return files_provider().is_directory(path, code);
}

/**
 * @brief Whether a path is a file, on the disk in use.
 * @param path The path.
 * @return Whether it is.
 */
inline bool is_regular_file(const std::filesystem::path& path) {
    std::error_code code;
    return files_provider().is_regular_file(path, code);
}

/**
 * @brief How big a file is, on the disk in use.
 * @param path The file.
 * @return Its size.
 */
inline std::uintmax_t file_size(const std::filesystem::path& path) {
    std::error_code code;
    return files_provider().file_size(path, code);
}

/**
 * @brief When a file was last written, on the disk in use.
 * @param path The file.
 * @return The time.
 */
inline std::filesystem::file_time_type last_write_time(const std::filesystem::path& path) {
    std::error_code code;
    return files_provider().last_write_time(path, code);
}

/**
 * @brief Remove a file or an empty folder, on the disk in use.
 * @param path The path.
 * @return Whether it was there.
 */
inline bool remove(const std::filesystem::path& path) {
    std::error_code code;
    return files_provider().remove(path, code);
}

/**
 * @brief Remove a folder and all that's in it, on the disk in use.
 * @param path The path.
 * @return How many went.
 */
inline std::uintmax_t remove_all(const std::filesystem::path& path) {
    std::error_code code;
    return files_provider().remove_all(path, code);
}

/**
 * @brief Move or rename, on the disk in use.
 * @param from Where it is.
 * @param to Where it goes.
 */
inline void rename(const std::filesystem::path& from, const std::filesystem::path& to) {
    std::error_code code;
    files_provider().rename(from, to, code);
}

/**
 * @brief Copy a file, on the disk in use.
 * @param from The file.
 * @param to Where the copy goes.
 * @param options What to do about one already there.
 * @return Whether it was copied.
 */
inline bool copy_file(const std::filesystem::path& from, const std::filesystem::path& to,
                      std::filesystem::copy_options options = std::filesystem::copy_options::none) {
    std::error_code code;
    return files_provider().copy_file(from, to, options, code);
}

/**
 * @brief Copy a file or folder, on the disk in use.
 * @param from What to copy.
 * @param to Where the copy goes.
 * @param options How.
 */
inline void copy(const std::filesystem::path& from, const std::filesystem::path& to, std::filesystem::copy_options options) {
    std::error_code code;
    files_provider().copy(from, to, options, code);
}

/**
 * @brief Whether two paths are the same file or folder, on the disk in use.
 * @param first One.
 * @param second The other.
 * @return Whether they are.
 */
inline bool equivalent(const std::filesystem::path& first, const std::filesystem::path& second) {
    std::error_code code;
    return files_provider().equivalent(first, second, code);
}

/**
 * @brief Open a file to write, on the disk in use: `*disk::write(path) << "bytes";` writes it whole.
 * @param path The file.
 * @param mode How, binary and emptied unless told otherwise.
 * @return The stream; what's written is in the file once it's gone.
 */
inline std::unique_ptr<std::ostream> write(const std::filesystem::path& path, std::ios::openmode mode = std::ios::binary | std::ios::trunc) {
    return files_provider().open_write(path, mode);
}

/**
 * @brief Open a file to read, on the disk in use.
 * @param path The file.
 * @param mode How, binary unless told otherwise.
 * @return The stream, failed when it can't be read.
 */
inline std::unique_ptr<std::istream> read(const std::filesystem::path& path, std::ios::openmode mode = std::ios::binary) {
    return files_provider().open_read(path, mode);
}

/**
 * @brief Everything in a file, on the disk in use.
 * @param path The file.
 * @return Its bytes, or empty when it can't be read.
 */
inline std::string contents(const std::filesystem::path& path) {
    const auto stream = read(path);
    return *stream ? std::string(std::istreambuf_iterator<char>(*stream), {}) : std::string();
}

/**
 * @brief What's in a folder, on the disk in use.
 * @param folder The folder.
 * @param recursive Whether to go into its folders.
 * @return Each file and folder.
 */
inline std::vector<FileEntry> list(const std::filesystem::path& folder, bool recursive = false) {
    std::error_code code;
    return files_provider().list(folder, recursive, code);
}

}

#endif
