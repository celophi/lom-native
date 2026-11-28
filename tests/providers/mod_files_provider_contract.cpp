#include "../../src/mods/mod_files_provider.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>
#include "../support/lom_check.h"

namespace
{

/**
 * @brief A new, empty folder for the test, in the system's temporary folder.
 * @return The folder.
 */
std::filesystem::path new_folder()
{
    std::error_code code;
    const std::filesystem::path folder = std::filesystem::temp_directory_path(code) / ("lom-mod-files-provider-" + std::to_string(std::rand()));
    std::filesystem::remove_all(folder, code);
    std::filesystem::create_directories(folder / "scripts" / "lib", code);
    return folder;
}

/**
 * @brief Write a whole file.
 * @param path The file.
 * @param bytes What goes in it.
 */
void write(const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream out(path, std::ios::binary);
    out << bytes;
}

/**
 * @brief Check a provider reads a mod's files byte for byte, measures them, and tells files from folders.
 * @param provider The provider.
 * @param folder A mod's folder holding mod.json ("{\"id\":\"dice\"}\0\xff") and scripts/lib/a.lua ("return 1").
 * @note Given a mod's folder with a file at its top and one two folders down, when the provider reads, measures and
 *       checks them, then it gets their bytes and sizes, sees the files as files and the folders as folders, and finds
 *       nothing where nothing is.
 */
void reads_a_mods_files(lom::mods::ModFilesProvider& provider, const std::filesystem::path& folder)
{
    const std::string manifest("{\"id\":\"dice\"}\0\xff", 15);
    std::string bytes;

    CHECK(provider.read(folder / "mod.json", bytes) && bytes == manifest);
    CHECK(provider.size(folder / "mod.json") == manifest.size());
    CHECK(provider.is_file(folder / "scripts" / "lib" / "a.lua"));
    CHECK(!provider.is_file(folder / "scripts"));
    CHECK(provider.is_folder(folder / "scripts"));
    CHECK(!provider.is_folder(folder / "mod.json"));
    CHECK(!provider.read(folder / "missing.lua", bytes));
    CHECK(!provider.size(folder / "missing.lua").has_value());
    CHECK(!provider.is_file(folder / "missing.lua"));
    CHECK(!provider.is_folder(folder / "missing"));
}

/**
 * @brief Check a provider lists every file under a mod's folder, relative to it, and fails for a missing folder.
 * @param provider The provider.
 * @param folder The mod's folder from reads_a_mods_files.
 * @note Given the same folder, when the provider lists it, then it gets mod.json and scripts/lib/a.lua and no folders;
 *       when it lists a folder that isn't there, then it fails.
 */
void lists_a_mods_files(lom::mods::ModFilesProvider& provider, const std::filesystem::path& folder)
{
    std::vector<std::filesystem::path> files;

    CHECK(provider.files_under(folder, files));
    std::vector<std::string> names;

    for (const std::filesystem::path& file : files)
    {
        names.push_back(file.generic_string());
    }

    std::sort(names.begin(), names.end());
    CHECK(names == (std::vector<std::string>{"mod.json", "scripts/lib/a.lua"}));
    std::vector<std::filesystem::path> none;
    CHECK(!provider.files_under(folder / "missing", none));
}

class StandIn : public lom::mods::ModFilesProvider
{
  public:
    bool read(const std::filesystem::path&, std::string&) override
    {
        return false;
    }

    std::optional<std::uintmax_t> size(const std::filesystem::path&) override
    {
        return std::nullopt;
    }

    bool is_file(const std::filesystem::path&) override
    {
        return false;
    }

    bool is_folder(const std::filesystem::path&) override
    {
        return false;
    }

    bool files_under(const std::filesystem::path&, std::vector<std::filesystem::path>&) override
    {
        return false;
    }
};

/**
 * @brief Check a provider put in its place is the one in use, and nullptr goes back to the disk's.
 * @note Given another provider is put in use, then it's the one handed out; when nullptr is put in use, then the
 *       disk's is again.
 */
void another_provider_can_stand_in_and_step_back()
{
    lom::mods::ModFilesProvider* disk = &lom::mods::mod_files_provider();
    StandIn stand_in;

    lom::mods::use_mod_files_provider(&stand_in);
    CHECK(&lom::mods::mod_files_provider() == &stand_in);
    lom::mods::use_mod_files_provider(nullptr);
    CHECK(&lom::mods::mod_files_provider() == disk);
}

}

int main()
{
    const std::filesystem::path folder = new_folder();
    write(folder / "mod.json", std::string("{\"id\":\"dice\"}\0\xff", 15));
    write(folder / "scripts" / "lib" / "a.lua", "return 1");
    reads_a_mods_files(lom::mods::mod_files_provider(), folder);
    lists_a_mods_files(lom::mods::mod_files_provider(), folder);
    std::error_code ignored;
    std::filesystem::remove_all(folder, ignored);

    another_provider_can_stand_in_and_step_back();
    std::puts("mod-files-provider: reads mods' files from the disk");
    return 0;
}
