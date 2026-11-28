#include "lom/launcher/common/remaster_install.h"
#include "lom/launcher/common/files_provider.h"

#include <cstdlib>
#include <istream>
#include <ostream>
#include <sstream>
#include <system_error>

namespace lom::launcher::RemasterInstall {

namespace {

constexpr char maps_folder[] = "hdmap";
constexpr char data_folder[] = "windata";
constexpr char library_file[] = "libraryfolders.vdf";
constexpr char path_key[] = "\"path\"";

/**
 * @brief Check whether a folder exists.
 * @param path The folder.
 * @return Whether it does.
 */
bool folder_exists(const std::filesystem::path& path) {
    std::error_code code;
    return files_provider().is_directory(path, code);
}

/** The quoted value after position @p from in a vdf line, with its \\ escapes undone. */
std::optional<std::string> quoted_after(const std::string& line, std::size_t from) {
    const std::size_t open = line.find('"', from);

    if (open == std::string::npos) {
        return std::nullopt;
    }

    std::string value;

    for (std::size_t i = open + 1; i < line.size(); ++i) {
        if (line[i] == '\\' && i + 1 < line.size()) {
            value += line[++i];
        } else if (line[i] == '"') {
            return value;
        } else {
            value += line[i];
        }
    }

    return std::nullopt;
}

}

bool looks_like(const std::filesystem::path& folder) {
    return !folder.empty() && (folder_exists(folder / data_folder / maps_folder) || folder_exists(folder / maps_folder));
}

std::vector<std::filesystem::path> parse_library_folders(const std::string& text) {
    std::vector<std::filesystem::path> folders;
    std::istringstream lines(text);
    std::string line;

    while (std::getline(lines, line)) {
        const std::size_t key = line.find(path_key);

        if (key == std::string::npos) {
            continue;
        }

        if (const auto value = quoted_after(line, key + sizeof(path_key) - 1); value && !value->empty()) {
            folders.push_back(std::filesystem::u8path(*value));
        }
    }

    return folders;
}

std::vector<std::filesystem::path> steam_libraries(const std::vector<std::filesystem::path>& steam_roots) {
    std::vector<std::filesystem::path> libraries;
    auto add = [&libraries](const std::filesystem::path& folder) {
        for (const auto& known : libraries) {
            if (known == folder) {
                return;
            }
        }

        libraries.push_back(folder);
    };

    for (const auto& root : steam_roots) {
        add(root);
    }

    for (const auto& root : steam_roots) {
        const auto file_file = files_provider().open_read(root / "steamapps" / library_file, std::ios::binary);
        std::istream& file = *file_file;

        if (!file) {
            continue;
        }

        std::ostringstream text;
        text << file.rdbuf();

        for (const auto& library : parse_library_folders(text.str())) {
            add(library);
        }
    }

    return libraries;
}

std::vector<std::filesystem::path> default_steam_roots(const std::filesystem::path& home) {
    std::vector<std::filesystem::path> roots;
#if defined(_WIN32)
    (void)home;
    for (const char* variable : {"ProgramFiles(x86)", "ProgramFiles"}) {
        if (const char* folder = std::getenv(variable); folder && *folder) {
            roots.push_back(std::filesystem::u8path(folder) / "Steam");
        }
    }
    roots.push_back("C:/Program Files (x86)/Steam");
#elif defined(__APPLE__)
    roots.push_back(home / "Library/Application Support/Steam");
#else
    roots.push_back(home / ".local/share/Steam");
    roots.push_back(home / ".steam/steam");
    roots.push_back(home / ".var/app/com.valvesoftware.Steam/.local/share/Steam");
#endif
    return roots;
}

std::optional<std::filesystem::path> find(const std::vector<std::filesystem::path>& steam_roots) {
    for (const auto& library : steam_libraries(steam_roots)) {
        const std::filesystem::path folder = library / "steamapps" / "common" / steam_folder;

        if (looks_like(folder)) {
            return folder;
        }
    }

    return std::nullopt;
}

}
