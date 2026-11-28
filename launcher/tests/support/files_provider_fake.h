#ifndef LOM_LAUNCHER_FILES_PROVIDER_FAKE_H
#define LOM_LAUNCHER_FILES_PROVIDER_FAKE_H

#include "lom/launcher/common/files_provider.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>

namespace lom::launcher::test {

class FilesProviderFake : public FilesProvider {
  public:
    struct Node {
        bool directory = false;
        std::string bytes;
        std::filesystem::file_time_type written{};
        std::filesystem::perms permissions = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;
        std::string link;
    };

    /**
     * @brief Start an empty disk, with only its root folder, and put it in use in place of the real one.
     */
    FilesProviderFake();

    /**
     * @brief Go back to the real disk.
     */
    ~FilesProviderFake() override;

    /**
     * @brief Put a file on the fake disk, making the folders it's in, as a test's given does.
     * @param path The file.
     * @param bytes What's in it.
     */
    void add_file(const std::filesystem::path& path, const std::string& bytes);

    /**
     * @brief Put a symbolic link on the fake disk, to a file or folder already there, as a test's given does.
     * @param path The link.
     * @param target What it points to.
     */
    void add_link(const std::filesystem::path& path, const std::filesystem::path& target);

    /**
     * @brief Put a folder on the fake disk, and the folders it's in.
     * @param path The folder.
     */
    void add_folder(const std::filesystem::path& path);

    /**
     * @brief Mark a file as last written at a time, as a test's given does.
     * @param path The file.
     * @param time When.
     */
    void set_last_write_time(const std::filesystem::path& path, std::filesystem::file_time_type time);

    /**
     * @brief What a file on the fake disk holds, for a test's then.
     * @param path The file.
     * @return Its bytes, or empty when it isn't there.
     */
    std::string contents(const std::filesystem::path& path) const;

    /**
     * @brief Whether a file or folder is on the fake disk, for a test's then.
     * @param path The path.
     * @return Whether it is.
     */
    bool has(const std::filesystem::path& path) const;

    /**
     * @brief A file's permissions on the fake disk, for a test's then.
     * @param path The file.
     * @return Its permissions, or none when it isn't there.
     */
    std::filesystem::perms permissions_of(const std::filesystem::path& path) const;

    std::uintmax_t free_space = std::uintmax_t(1) << 40;

    bool exists(const std::filesystem::path& path, std::error_code& code) const override;
    bool is_directory(const std::filesystem::path& path, std::error_code& code) const override;
    bool is_regular_file(const std::filesystem::path& path, std::error_code& code) const override;
    std::uintmax_t file_size(const std::filesystem::path& path, std::error_code& code) const override;
    std::filesystem::file_time_type last_write_time(const std::filesystem::path& path, std::error_code& code) const override;
    std::vector<FileEntry> list(const std::filesystem::path& folder, bool recursive, std::error_code& code) const override;
    bool create_directories(const std::filesystem::path& path, std::error_code& code) override;
    bool remove(const std::filesystem::path& path, std::error_code& code) override;
    std::uintmax_t remove_all(const std::filesystem::path& path, std::error_code& code) override;
    void rename(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& code) override;
    bool copy_file(const std::filesystem::path& from, const std::filesystem::path& to, std::filesystem::copy_options options, std::error_code& code) override;
    void copy(const std::filesystem::path& from, const std::filesystem::path& to, std::filesystem::copy_options options, std::error_code& code) override;
    std::filesystem::space_info space(const std::filesystem::path& path, std::error_code& code) const override;
    std::filesystem::path weakly_canonical(const std::filesystem::path& path, std::error_code& code) const override;
    bool equivalent(const std::filesystem::path& first, const std::filesystem::path& second, std::error_code& code) const override;
    void permissions(const std::filesystem::path& path, std::filesystem::perms permissions, std::error_code& code) override;
    std::unique_ptr<std::istream> open_read(const std::filesystem::path& path, std::ios::openmode mode) const override;
    std::unique_ptr<std::ostream> open_write(const std::filesystem::path& path, std::ios::openmode mode) override;

    /**
     * @brief Keep what a stream wrote, as the disk does when it's flushed or closed.
     * @param key The file's key.
     * @param bytes All it holds now.
     */
    void commit(const std::string& key, const std::string& bytes);

  private:
    static std::string key(const std::filesystem::path& path);
    static std::string parent_key(const std::string& key);
    bool folder_holds_anything(const std::string& key) const;
    void make_folders(const std::string& key);
    std::filesystem::file_time_type tick();

    mutable std::recursive_mutex mutex_;
    std::map<std::string, Node> nodes_;
    std::filesystem::file_time_type clock_;
};

}

#endif
