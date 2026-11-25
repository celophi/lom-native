#ifndef LOM_LAUNCHER_LAUNCHER_SETTINGS_H
#define LOM_LAUNCHER_LAUNCHER_SETTINGS_H

#include "lom/launcher/disc/installation.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace lom::launcher {
class LauncherSettings {
  public:
    enum class Theme { System, Light, Dark };

    enum class LoadStatus { Loaded, Missing, Corrupt };

    static constexpr int schema_version = 1;

    /**
     * @brief Load settings, keeping defaults for absent fields. The active
     *        installation is also kept in installations, under its release,
     *        with any other edition set up.
     * @param path Settings file.
     * @param error Receives a description when Corrupt; may be null.
     * @return Loaded, Missing (defaults kept) or Corrupt (defaults kept, file renamed aside).
     */
    LoadStatus load(const std::filesystem::path& path, std::string* error);

    /**
     * @brief Save settings atomically.
     * @param path Settings file.
     * @param error Receives a description on failure; may be null.
     * @return True when the complete file was written.
     */
    bool save(const std::filesystem::path& path, std::string* error) const;

    /**
     * @brief Read only the launcher's language from a settings file, changing nothing (the window needs it before it loads the rest).
     * @param path Settings file.
     * @return The language's code, or empty for the system's (or when the file is missing or unreadable).
     */
    static std::string read_language(const std::filesystem::path& path);

    /**
     * @brief Read the remembered save folder of a release.
     * @param release_id Release identifier.
     * @return The folder, or empty when none was chosen.
     */
    std::filesystem::path save_root(const std::string& release_id) const;

    bool ownership_notice_acknowledged = false;
    Theme theme = Theme::System;
    std::filesystem::path games_root;
    std::optional<Installation> active_installation;
    std::map<std::string, Installation> installations;
    std::map<std::string, std::filesystem::path> save_roots;
    std::filesystem::path last_image_directory;
    bool check_updates_on_startup = true;
    std::string last_seen_version;
    bool record_sessions = false;
    bool use_remaster = true;
    std::filesystem::path remaster_root;
    std::string language;
};

}

#endif
