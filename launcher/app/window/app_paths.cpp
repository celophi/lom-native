#include "window/app_paths.h"
#include "lom/launcher/common/files_provider.h"

#include "lom/launcher/common/sha256.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

#include <system_error>
#include <vector>

namespace lom::launcher {

namespace {

constexpr char settings_name[] = "launcher.json";
constexpr char games_name[] = "games";
constexpr char logs_name[] = "logs";
constexpr char launcher_log_name[] = "launcher.log";
constexpr char bug_reports_name[] = "bug-reports";
constexpr char sessions_name[] = "sessions";
constexpr char mods_name[] = "mods";
constexpr char snapshots_name[] = "verification";
constexpr char snapshot_suffix[] = ".json";
constexpr std::size_t snapshot_name_digits = 24;
constexpr char catalog_name[] = "supported-discs.json";
constexpr char us_cards_name[] = "cards";
constexpr char region_cards_prefix[] = "cards-";
constexpr char us_release[] = "us";
constexpr char resource_directory_name[] = "launcher-data";
constexpr char shared_resources[] = "../share/lom-native";
constexpr char bundle_resources[] = "../Resources";
constexpr char bundle_executable_folder[] = "MacOS";
constexpr char bundle_contents_folder[] = "Contents";
constexpr char bundle_extension[] = ".app";
constexpr char executable_override[] = "LOM_GAME_EXECUTABLE";
constexpr char resource_override[] = "LOM_LAUNCHER_DATA";
#if defined(_WIN32)
constexpr char executable_suffix[] = ".exe";
#else
constexpr char executable_suffix[] = "";
#endif

/**
 * @brief Find the directory holding launcher data beside the executable, in a Linux package's share/, or in a macOS bundle.
 * @param program Directory of the launcher executable.
 * @return Resource directory.
 */
std::filesystem::path find_resources(const std::filesystem::path& program) {
    const QByteArray override_path = qgetenv(resource_override);

    if (!override_path.isEmpty()) {
        return AppPaths::to_path(QString::fromLocal8Bit(override_path));
    }

    std::error_code code;

    for (const std::filesystem::path& candidate : {program / resource_directory_name, program / shared_resources / resource_directory_name}) {
        if (files_provider().exists(candidate / catalog_name, code)) {
            return candidate;
        }
    }

    return program / bundle_resources / resource_directory_name;
}

/**
 * @brief List the directories searched for the game executable, in order.
 * @param program Directory searched first: the launcher's own.
 * @return program, then on macOS the folder holding the launcher's bundle when it runs from one.
 */
std::vector<std::filesystem::path> game_directories(const std::filesystem::path& program) {
    std::vector<std::filesystem::path> directories{program};
#if defined(__APPLE__)
    const std::filesystem::path contents = program.parent_path();

    if (program.filename() == bundle_executable_folder && contents.filename() == bundle_contents_folder &&
        contents.parent_path().extension() == bundle_extension) {
        directories.push_back(contents.parent_path().parent_path());
    }
#endif
    return directories;
}

}

AppPaths::AppPaths(std::filesystem::path config_directory, std::filesystem::path data_directory, std::filesystem::path resource_directory,
                   std::filesystem::path program_directory)
    : config_directory_(std::move(config_directory)), data_directory_(std::move(data_directory)), resource_directory_(std::move(resource_directory)),
      program_directory_(std::move(program_directory)) {
}

AppPaths AppPaths::standard() {
    return for_program(to_path(QCoreApplication::applicationDirPath()));
}

AppPaths AppPaths::for_program(const std::filesystem::path& program) {
    std::error_code code;

    if (files_provider().exists(program / portable_marker, code)) {
        const std::filesystem::path user = program / portable_folder;
        files_provider().create_directories(user, code);
        return AppPaths(user, user, find_resources(program), program);
    }

    return AppPaths(to_path(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)),
                    to_path(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)), find_resources(program), program);
}

std::filesystem::path AppPaths::to_path(const QString& text) {
#if defined(_WIN32)
    return std::filesystem::path(QDir::toNativeSeparators(text).toStdWString());
#else
    return std::filesystem::u8path(text.toStdString());
#endif
}

QString AppPaths::to_text(const std::filesystem::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.u8string());
#endif
}

std::filesystem::path AppPaths::settings_file() const {
    return config_directory_ / settings_name;
}

std::filesystem::path AppPaths::default_games_root() const {
    return data_directory_ / games_name;
}

std::filesystem::path AppPaths::default_save_root(const std::string& release_id) const {
    return data_directory_ / (release_id == us_release ? std::string(us_cards_name) : region_cards_prefix + release_id);
}

std::filesystem::path AppPaths::snapshot_file(const std::filesystem::path& data_path, const std::string& manifest_sha256) const {
    Sha256 hash;
    const std::string key = data_path.u8string() + '\n' + manifest_sha256;
    hash.update(key.data(), key.size());
    return data_directory_ / snapshots_name / (hash.hex_digest().substr(0, snapshot_name_digits) + snapshot_suffix);
}

std::filesystem::path AppPaths::logs_directory() const {
    return data_directory_ / logs_name;
}

std::filesystem::path AppPaths::launcher_log_file() const {
    return logs_directory() / launcher_log_name;
}

std::filesystem::path AppPaths::bug_reports_directory() const {
    return data_directory_ / bug_reports_name;
}

std::filesystem::path AppPaths::sessions_directory() const {
    return data_directory_ / sessions_name;
}

std::filesystem::path AppPaths::mods_directory() const {
    return data_directory_ / mods_name;
}

std::filesystem::path AppPaths::bundled_mods_directory() const {
    return resource_directory_ / mods_name;
}

std::filesystem::path AppPaths::catalog_file() const {
    return resource_directory_ / catalog_name;
}

std::filesystem::path AppPaths::game_executable(const std::string& runtime) const {
    if (runtime.empty()) {
        return {};
    }

    const QByteArray override_path = qgetenv(executable_override);

    if (!override_path.isEmpty()) {
        return to_path(QString::fromLocal8Bit(override_path));
    }

    std::error_code code;

    for (const std::filesystem::path& directory : game_directories(program_directory_)) {
        const std::filesystem::path candidate = directory / (runtime + executable_suffix);

        if (files_provider().is_regular_file(candidate, code)) {
            return candidate;
        }
    }

    return {};
}

const std::filesystem::path& AppPaths::data_directory() const {
    return data_directory_;
}

}
