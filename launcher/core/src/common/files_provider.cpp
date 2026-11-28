#include "lom/launcher/common/files_provider.h"

#include <atomic>
#include <fstream>

namespace lom::launcher {

namespace {

class DiskFilesProvider : public FilesProvider {
  public:
    bool exists(const std::filesystem::path& path, std::error_code& code) const override {
        return std::filesystem::exists(path, code);
    }

    bool is_directory(const std::filesystem::path& path, std::error_code& code) const override {
        return std::filesystem::is_directory(path, code);
    }

    bool is_regular_file(const std::filesystem::path& path, std::error_code& code) const override {
        return std::filesystem::is_regular_file(path, code);
    }

    std::uintmax_t file_size(const std::filesystem::path& path, std::error_code& code) const override {
        return std::filesystem::file_size(path, code);
    }

    std::filesystem::file_time_type last_write_time(const std::filesystem::path& path, std::error_code& code) const override {
        return std::filesystem::last_write_time(path, code);
    }

    std::vector<FileEntry> list(const std::filesystem::path& folder, bool recursive, std::error_code& code) const override {
        std::vector<FileEntry> entries;
        const auto add = [&entries](const std::filesystem::directory_entry& entry) {
            std::error_code ignored;
            FileEntry found;
            found.path = entry.path();
            found.directory = entry.is_directory(ignored);
            found.regular = entry.is_regular_file(ignored);
            found.symlink = entry.is_symlink(ignored);
            found.size = found.regular ? entry.file_size(ignored) : 0;
            entries.push_back(found);
        };

        if (recursive) {
            for (std::filesystem::recursive_directory_iterator it(folder, code), end; !code && it != end; it.increment(code)) {
                add(*it);
            }
        } else {
            for (std::filesystem::directory_iterator it(folder, code), end; !code && it != end; it.increment(code)) {
                add(*it);
            }
        }

        return entries;
    }

    bool create_directories(const std::filesystem::path& path, std::error_code& code) override {
        return std::filesystem::create_directories(path, code);
    }

    bool remove(const std::filesystem::path& path, std::error_code& code) override {
        return std::filesystem::remove(path, code);
    }

    std::uintmax_t remove_all(const std::filesystem::path& path, std::error_code& code) override {
        return std::filesystem::remove_all(path, code);
    }

    void rename(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& code) override {
        std::filesystem::rename(from, to, code);
    }

    bool copy_file(const std::filesystem::path& from, const std::filesystem::path& to, std::filesystem::copy_options options, std::error_code& code) override {
        return std::filesystem::copy_file(from, to, options, code);
    }

    void copy(const std::filesystem::path& from, const std::filesystem::path& to, std::filesystem::copy_options options, std::error_code& code) override {
        std::filesystem::copy(from, to, options, code);
    }

    std::filesystem::space_info space(const std::filesystem::path& path, std::error_code& code) const override {
        return std::filesystem::space(path, code);
    }

    std::filesystem::path weakly_canonical(const std::filesystem::path& path, std::error_code& code) const override {
        return std::filesystem::weakly_canonical(path, code);
    }

    bool equivalent(const std::filesystem::path& first, const std::filesystem::path& second, std::error_code& code) const override {
        return std::filesystem::equivalent(first, second, code);
    }

    void permissions(const std::filesystem::path& path, std::filesystem::perms permissions, std::error_code& code) override {
        std::filesystem::permissions(path, permissions, code);
    }

    std::unique_ptr<std::istream> open_read(const std::filesystem::path& path, std::ios::openmode mode) const override {
        return std::make_unique<std::ifstream>(path, mode | std::ios::in);
    }

    std::unique_ptr<std::ostream> open_write(const std::filesystem::path& path, std::ios::openmode mode) override {
        return std::make_unique<std::ofstream>(path, mode | std::ios::out);
    }
};

DiskFilesProvider disk;
std::atomic<FilesProvider*> in_use{nullptr};

}

FilesProvider& files_provider() {
    FilesProvider* provider = in_use.load();
    return provider ? *provider : disk;
}

void use_files_provider(FilesProvider* provider) {
    in_use.store(provider);
}

}
