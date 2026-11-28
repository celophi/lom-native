#ifndef LOM_LAUNCHER_FILES_PROVIDER_H
#define LOM_LAUNCHER_FILES_PROVIDER_H

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <ios>
#include <memory>
#include <system_error>
#include <vector>

namespace lom::launcher {

struct FileEntry {
    std::filesystem::path path;
    bool directory = false;
    bool regular = false;
    bool symlink = false;
    std::uintmax_t size = 0;
};

class FilesProvider {
  public:
    /**
     * @brief Destroy it.
     */
    virtual ~FilesProvider() = default;

    /**
     * @brief Whether something is at a path, as std::filesystem::exists.
     * @param path The path.
     * @param code Set when it can't be told.
     * @return Whether it is.
     */
    virtual bool exists(const std::filesystem::path& path, std::error_code& code) const = 0;

    /**
     * @brief Whether a path is a folder, as std::filesystem::is_directory.
     * @param path The path.
     * @param code Set when it can't be told.
     * @return Whether it is.
     */
    virtual bool is_directory(const std::filesystem::path& path, std::error_code& code) const = 0;

    /**
     * @brief Whether a path is a file, as std::filesystem::is_regular_file.
     * @param path The path.
     * @param code Set when it can't be told.
     * @return Whether it is.
     */
    virtual bool is_regular_file(const std::filesystem::path& path, std::error_code& code) const = 0;

    /**
     * @brief How big a file is, as std::filesystem::file_size.
     * @param path The file.
     * @param code Set when it can't be told.
     * @return Its size.
     */
    virtual std::uintmax_t file_size(const std::filesystem::path& path, std::error_code& code) const = 0;

    /**
     * @brief When a file was last written, as std::filesystem::last_write_time.
     * @param path The file.
     * @param code Set when it can't be told.
     * @return The time.
     */
    virtual std::filesystem::file_time_type last_write_time(const std::filesystem::path& path, std::error_code& code) const = 0;

    /**
     * @brief What's in a folder, as std::filesystem's directory iterators find it.
     * @param folder The folder.
     * @param recursive Whether to go into the folders in it too.
     * @param code Set when it can't be listed.
     * @return Each file and folder, in no particular order.
     */
    virtual std::vector<FileEntry> list(const std::filesystem::path& folder, bool recursive, std::error_code& code) const = 0;

    /**
     * @brief Make a folder and any folders it's in, as std::filesystem::create_directories.
     * @param path The folder.
     * @param code Set when it can't be made.
     * @return Whether any were made.
     */
    virtual bool create_directories(const std::filesystem::path& path, std::error_code& code) = 0;

    /**
     * @brief Remove a file or an empty folder, as std::filesystem::remove.
     * @param path The path.
     * @param code Set when it can't be removed.
     * @return Whether it was there.
     */
    virtual bool remove(const std::filesystem::path& path, std::error_code& code) = 0;

    /**
     * @brief Remove a file or a folder and all that's in it, as std::filesystem::remove_all.
     * @param path The path.
     * @param code Set when it can't be removed.
     * @return How many files and folders went.
     */
    virtual std::uintmax_t remove_all(const std::filesystem::path& path, std::error_code& code) = 0;

    /**
     * @brief Move or rename a file or folder, as std::filesystem::rename.
     * @param from Where it is.
     * @param to Where it goes.
     * @param code Set when it can't be.
     */
    virtual void rename(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& code) = 0;

    /**
     * @brief Copy a file, as std::filesystem::copy_file.
     * @param from The file.
     * @param to Where the copy goes.
     * @param options What to do about a file already there.
     * @param code Set when it can't be.
     * @return Whether it was copied.
     */
    virtual bool copy_file(const std::filesystem::path& from, const std::filesystem::path& to, std::filesystem::copy_options options, std::error_code& code) = 0;

    /**
     * @brief Copy a file or folder, as std::filesystem::copy.
     * @param from What to copy.
     * @param to Where the copy goes.
     * @param options How, like whether into the folders in it.
     * @param code Set when it can't be.
     */
    virtual void copy(const std::filesystem::path& from, const std::filesystem::path& to, std::filesystem::copy_options options, std::error_code& code) = 0;

    /**
     * @brief How much room there is where a path is, as std::filesystem::space.
     * @param path The path.
     * @param code Set when it can't be told.
     * @return The room.
     */
    virtual std::filesystem::space_info space(const std::filesystem::path& path, std::error_code& code) const = 0;

    /**
     * @brief A path with its links and dots worked out as far as it exists, as std::filesystem::weakly_canonical.
     * @param path The path.
     * @param code Set when it can't be.
     * @return The path.
     */
    virtual std::filesystem::path weakly_canonical(const std::filesystem::path& path, std::error_code& code) const = 0;

    /**
     * @brief Whether two paths are the same file or folder, as std::filesystem::equivalent.
     * @param first One.
     * @param second The other.
     * @param code Set when it can't be told.
     * @return Whether they are.
     */
    virtual bool equivalent(const std::filesystem::path& first, const std::filesystem::path& second, std::error_code& code) const = 0;

    /**
     * @brief Set a file's permissions, as std::filesystem::permissions with its options left as they are.
     * @param path The file.
     * @param permissions Its permissions.
     * @param code Set when they can't be.
     */
    virtual void permissions(const std::filesystem::path& path, std::filesystem::perms permissions, std::error_code& code) = 0;

    /**
     * @brief Open a file to read, as std::ifstream does.
     * @param path The file.
     * @param mode How, binary as a rule.
     * @return The stream, failed when it can't be opened.
     */
    virtual std::unique_ptr<std::istream> open_read(const std::filesystem::path& path, std::ios::openmode mode) const = 0;

    /**
     * @brief Open a file to write, as std::ofstream does, made or emptied unless it's opened to add to.
     * @param path The file.
     * @param mode How: binary, and whether to add to the end.
     * @return The stream, failed when it can't be opened. What's written is in the file once the stream is flushed or
     *         gone.
     */
    virtual std::unique_ptr<std::ostream> open_write(const std::filesystem::path& path, std::ios::openmode mode) = 0;
};

/**
 * @brief The provider in use for the launcher's files: the disk's, unless a test put in its own.
 * @return The provider.
 * @note This is the launcher's files provider: the one place it reaches the disk, every call a straight pass-through
 *       to std::filesystem and the standard streams. Everything the launcher does with files (importing discs and
 *       saves, settings, mods, updates) is its own code, which tests run for real with a provider of their own that
 *       keeps files in memory (launcher/tests/support/files_provider_fake.h). tools/testing/provider_check.py keeps the
 *       rest of the launcher from going around it.
 */
FilesProvider& files_provider();

/**
 * @brief Use another provider, as tests do.
 * @param provider The provider, or nullptr to go back to the disk's. It has to stay valid while it's in use.
 */
void use_files_provider(FilesProvider* provider);

}

#endif
