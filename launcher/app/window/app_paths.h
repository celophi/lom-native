#ifndef LOM_LAUNCHER_APP_PATHS_H
#define LOM_LAUNCHER_APP_PATHS_H

#include <QString>

#include <filesystem>
#include <string>

namespace lom::launcher {
class AppPaths {
  public:
    static constexpr char application_name[] = "lom-native";

    /**
     * @brief Build paths from explicit roots.
     * @param config_directory Directory for launcher settings.
     * @param data_directory Per-user local data directory (games, cards, logs).
     * @param resource_directory Directory holding supported-discs.json and manifests.
     * @param program_directory Directory searched for game executables (on macOS, the folder holding its bundle is searched next).
     */
    AppPaths(std::filesystem::path config_directory, std::filesystem::path data_directory, std::filesystem::path resource_directory,
             std::filesystem::path program_directory);

    /**
     * @brief Build the platform's standard paths; requires the application name to be set.
     * @return Standard paths.
     */
    static AppPaths standard();

    static constexpr char portable_marker[] = "portable.txt";
    static constexpr char portable_folder[] = "user";

    /**
     * @brief Build the paths for a launcher in this folder: the standard ones, or in portable mode the user folder beside it.
     * @param program Directory of the launcher executable.
     * @return Paths.
     */
    static AppPaths for_program(const std::filesystem::path& program);

    /**
     * @brief Convert a Qt path string to a filesystem path without losing Unicode.
     * @param text Native or Qt path text.
     */
    static std::filesystem::path to_path(const QString& text);

    /**
     * @brief Convert a filesystem path to display text with native separators.
     * @param path Path.
     */
    static QString to_text(const std::filesystem::path& path);

    /**
     * @brief Report the launcher settings file.
     * @return Settings path.
     */
    std::filesystem::path settings_file() const;

    /**
     * @brief Report the default root for managed game files.
     */
    std::filesystem::path default_games_root() const;

    /**
     * @brief Report the default save folder of a release. The US default is the game's existing card location.
     * @param release_id Release identifier.
     */
    std::filesystem::path default_save_root(const std::string& release_id) const;

    /**
     * @brief Report where the verification snapshot of a game folder is kept.
     * @param data_path Verified game folder.
     * @param manifest_sha256 Aggregate identity of the manifest it matched.
     * @return Snapshot file in the launcher's data folder.
     */
    std::filesystem::path snapshot_file(const std::filesystem::path& data_path, const std::string& manifest_sha256) const;

    /**
     * @brief Report the log directory.
     */
    std::filesystem::path logs_directory() const;

    /**
     * @brief Report the launcher's own log.
     */
    std::filesystem::path launcher_log_file() const;

    /**
     * @brief Report where bug report zips are saved.
     */
    std::filesystem::path bug_reports_directory() const;

    /**
     * @brief Report where the game records play sessions when that's turned on.
     */
    std::filesystem::path sessions_directory() const;

    /**
     * @brief Report the mods folder: the user data folder's mods, which is
     *        where the game looks when it's started by hand too.
     */
    std::filesystem::path mods_directory() const;

    /**
     * @brief Report where the example mods that come with lom-native are: the
     *        launcher's resources' mods.
     */
    std::filesystem::path bundled_mods_directory() const;

    /**
     * @brief Report the release catalog file.
     * @return supported-discs.json path.
     */
    std::filesystem::path catalog_file() const;

    /**
     * @brief Find a game executable, honoring the LOM_GAME_EXECUTABLE override.
     * @param runtime Executable base name from the catalog.
     * @return The executable, or empty when not found.
     */
    std::filesystem::path game_executable(const std::string& runtime) const;

    /**
     * @brief Report the per-user local data directory.
     */
    const std::filesystem::path& data_directory() const;

  private:
    std::filesystem::path config_directory_;
    std::filesystem::path data_directory_;
    std::filesystem::path resource_directory_;
    std::filesystem::path program_directory_;
};

}

#endif
