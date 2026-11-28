#ifndef LOM_MOD_FILES_PROVIDER_FAKE_H
#define LOM_MOD_FILES_PROVIDER_FAKE_H

#include "../../../src/mods/mod_files_provider.h"

#include <map>
#include <string>

class ModFilesProviderFake : public lom::mods::ModFilesProvider
{
  public:
    /**
     * @brief Put the fake in use, with no files: mods' files served from memory in place of the disk, a file there
     *        when a test has added it and a folder there when a file is in it.
     */
    ModFilesProviderFake()
    {
        lom::mods::use_mod_files_provider(this);
    }

    /**
     * @brief Go back to the disk.
     */
    ~ModFilesProviderFake() override
    {
        lom::mods::use_mod_files_provider(nullptr);
    }

    /**
     * @brief Add a file, as a test's given does.
     * @param path The file.
     * @param bytes What's in it.
     */
    void add_file(const std::filesystem::path& path, const std::string& bytes)
    {
        files_[path.lexically_normal().generic_string()] = bytes;
    }

    bool read(const std::filesystem::path& path, std::string& bytes) override
    {
        const auto found = files_.find(path.lexically_normal().generic_string());

        if (found == files_.end())
        {
            return false;
        }

        bytes = found->second;
        return true;
    }

    std::optional<std::uintmax_t> size(const std::filesystem::path& path) override
    {
        const auto found = files_.find(path.lexically_normal().generic_string());
        return found == files_.end() ? std::nullopt : std::optional<std::uintmax_t>(found->second.size());
    }

    bool is_file(const std::filesystem::path& path) override
    {
        return files_.count(path.lexically_normal().generic_string()) != 0;
    }

    bool is_folder(const std::filesystem::path& path) override
    {
        const std::string inside = (path / "").lexically_normal().generic_string();
        const auto after = files_.lower_bound(inside);
        return after != files_.end() && after->first.compare(0, inside.size(), inside) == 0;
    }

    bool files_under(const std::filesystem::path& folder, std::vector<std::filesystem::path>& files) override
    {
        if (!is_folder(folder))
        {
            return false;
        }

        const std::string inside = (folder / "").lexically_normal().generic_string();

        for (auto file = files_.lower_bound(inside); file != files_.end() && file->first.compare(0, inside.size(), inside) == 0; ++file)
        {
            files.push_back(std::filesystem::path(file->first.substr(inside.size())));
        }

        return true;
    }

  private:
    std::map<std::string, std::string> files_;
};

#endif
