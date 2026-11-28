#include "mod_files_provider.h"

#include <atomic>
#include <fstream>
#include <sstream>
#include <system_error>

namespace lom::mods
{

namespace
{

class DiskModFilesProvider : public ModFilesProvider
{
  public:
    bool read(const std::filesystem::path& path, std::string& bytes) override
    {
        std::ifstream input(path, std::ios::binary);

        if (!input)
        {
            return false;
        }

        std::ostringstream all;
        all << input.rdbuf();
        bytes = all.str();
        return true;
    }

    std::optional<std::uintmax_t> size(const std::filesystem::path& path) override
    {
        std::error_code failed;
        const std::uintmax_t bytes = std::filesystem::file_size(path, failed);
        return failed ? std::nullopt : std::optional<std::uintmax_t>(bytes);
    }

    bool is_file(const std::filesystem::path& path) override
    {
        std::error_code ignored;
        return std::filesystem::is_regular_file(path, ignored);
    }

    bool is_folder(const std::filesystem::path& path) override
    {
        std::error_code ignored;
        return std::filesystem::is_directory(path, ignored);
    }

    bool files_under(const std::filesystem::path& folder, std::vector<std::filesystem::path>& files) override
    {
        std::error_code failed;

        for (auto entry = std::filesystem::recursive_directory_iterator(folder, failed); !failed && entry != std::filesystem::recursive_directory_iterator();
             entry.increment(failed))
        {
            if (entry->is_regular_file(failed))
            {
                files.push_back(std::filesystem::relative(entry->path(), folder, failed));
            }
        }

        return !failed;
    }
};

DiskModFilesProvider disk;
std::atomic<ModFilesProvider*> in_use{nullptr};

}

ModFilesProvider& mod_files_provider()
{
    ModFilesProvider* provider = in_use.load();
    return provider ? *provider : disk;
}

void use_mod_files_provider(ModFilesProvider* provider)
{
    in_use.store(provider);
}

}
