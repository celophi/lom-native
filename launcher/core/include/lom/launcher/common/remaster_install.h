#ifndef LOM_LAUNCHER_REMASTER_INSTALL_H
#define LOM_LAUNCHER_REMASTER_INSTALL_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lom::launcher {
namespace RemasterInstall {
inline constexpr char steam_folder[] = "Legend of Mana";

/**
 * @brief Whether a folder is the Remaster's install: it, or its windata
 *        folder, has the HD maps (hdmap).
 */
bool looks_like(const std::filesystem::path& folder);

/**
 * @brief Steam's library folders, from steamapps/libraryfolders.vdf under
 *        each Steam folder, starting with the Steam folders themselves.
 * @param steam_roots Where Steam is installed.
 */
std::vector<std::filesystem::path> steam_libraries(const std::vector<std::filesystem::path>& steam_roots);

/**
 * @brief The library paths a libraryfolders.vdf lists, in order.
 * @param text The file's text.
 */
std::vector<std::filesystem::path> parse_library_folders(const std::string& text);

/**
 * @brief Where Steam usually is on this system.
 * @param home The user's home folder.
 */
std::vector<std::filesystem::path> default_steam_roots(const std::filesystem::path& home);

/**
 * @brief The Remaster's folder in any of Steam's libraries.
 * @param steam_roots Where Steam is installed.
 * @return Its folder (steamapps/common/Legend of Mana), or nothing.
 */
std::optional<std::filesystem::path> find(const std::vector<std::filesystem::path>& steam_roots);

}

}

#endif
