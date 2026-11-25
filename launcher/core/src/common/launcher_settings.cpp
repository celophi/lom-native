#include "lom/launcher/common/launcher_settings.h"

#include "lom/launcher/common/atomic_file.h"
#include "lom/launcher/common/json.h"

#include <chrono>
#include <system_error>

namespace lom::launcher {

namespace {

constexpr char key_schema[] = "schema";
constexpr char key_acknowledged[] = "ownership_notice_acknowledged";
constexpr char key_theme[] = "theme";
constexpr char key_games_root[] = "games_root";
constexpr char key_installation[] = "active_installation";
constexpr char key_save_roots[] = "save_roots";
constexpr char key_installations[] = "installations";
constexpr char key_last_image_directory[] = "last_image_directory";
constexpr char key_check_updates[] = "check_updates_on_startup";
constexpr char key_last_seen_version[] = "last_seen_version";
constexpr char key_record_sessions[] = "record_sessions";
constexpr char key_use_remaster[] = "use_remaster";
constexpr char key_remaster_root[] = "remaster_root";
constexpr char key_language[] = "language";
constexpr char theme_system[] = "system";
constexpr char theme_light[] = "light";
constexpr char theme_dark[] = "dark";
constexpr char corrupt_suffix[] = ".corrupt-";

/**
 * @brief Read an optional path member.
 * @param document Settings object.
 * @param key Member name.
 * @return The path, or empty.
 */
std::filesystem::path path_member(const Json& document, const char* key) {
    const Json* value = document.find(key);
    return value == nullptr || value->as_string().empty() ? std::filesystem::path() : std::filesystem::u8path(value->as_string());
}

}

LauncherSettings::LoadStatus LauncherSettings::load(const std::filesystem::path& path, std::string* error) {
    std::error_code code;

    if (!std::filesystem::exists(path, code)) {
        return LoadStatus::Missing;
    }

    std::string problem;
    const auto text = AtomicFile::read(path, &problem);
    std::optional<Json> document;

    if (text) {
        document = Json::parse(*text, &problem);
    }

    if (!document || document->type() != Json::Type::Object) {
        if (problem.empty()) {
            problem = "the settings file is not a JSON object";
        }

        const auto stamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::filesystem::path aside = path;
        aside += corrupt_suffix + std::to_string(stamp);
        std::filesystem::rename(path, aside, code);

        if (error != nullptr) {
            *error = problem + (code ? "" : "; the unreadable file was kept as " + aside.filename().u8string());
        }

        return LoadStatus::Corrupt;
    }

    if (const Json* acknowledged = document->find(key_acknowledged)) {
        ownership_notice_acknowledged = acknowledged->as_bool(false);
    }

    if (const Json* value = document->find(key_theme)) {
        theme = value->as_string() == theme_light ? Theme::Light : value->as_string() == theme_dark ? Theme::Dark : Theme::System;
    }

    games_root = path_member(*document, key_games_root);
    last_image_directory = path_member(*document, key_last_image_directory);

    if (const Json* installation = document->find(key_installation)) {
        active_installation = Installation::from_json(*installation);
    }

    if (const Json* set_up = document->find(key_installations); set_up && set_up->type() == Json::Type::Object) {
        for (const auto& [release, value] : set_up->as_object()) {
            const std::optional<Installation> installation = Installation::from_json(value);

            if (installation && installation->release_id == release) {
                installations[release] = *installation;
            }
        }
    }

    if (active_installation) {
        installations[active_installation->release_id] = *active_installation;
    }

    if (const Json* roots = document->find(key_save_roots)) {
        for (const auto& [release, folder] : roots->as_object()) {
            if (!folder.as_string().empty()) {
                save_roots[release] = std::filesystem::u8path(folder.as_string());
            }
        }
    }

    if (const Json* updates = document->find(key_check_updates)) {
        check_updates_on_startup = updates->as_bool(true);
    }

    if (const Json* seen = document->find(key_last_seen_version)) {
        last_seen_version = seen->as_string();
    }

    if (const Json* record = document->find(key_record_sessions)) {
        record_sessions = record->as_bool(false);
    }

    if (const Json* remaster = document->find(key_use_remaster)) {
        use_remaster = remaster->as_bool(true);
    }

    remaster_root = path_member(*document, key_remaster_root);

    if (const Json* value = document->find(key_language)) {
        language = value->as_string();
    }

    return LoadStatus::Loaded;
}

std::string LauncherSettings::read_language(const std::filesystem::path& path) {
    std::error_code code;

    if (!std::filesystem::exists(path, code)) {
        return std::string();
    }

    const auto text = AtomicFile::read(path, nullptr);
    const std::optional<Json> document = text ? Json::parse(*text, nullptr) : std::nullopt;
    const Json* value = document && document->type() == Json::Type::Object ? document->find(key_language) : nullptr;
    return value == nullptr ? std::string() : value->as_string();
}

bool LauncherSettings::save(const std::filesystem::path& path, std::string* error) const {
    Json document;
    document[key_schema] = schema_version;
    document[key_acknowledged] = ownership_notice_acknowledged;
    document[key_theme] = theme == Theme::Light ? theme_light : theme == Theme::Dark ? theme_dark : theme_system;
    document[key_games_root] = games_root.u8string();
    document[key_last_image_directory] = last_image_directory.u8string();
    document[key_check_updates] = check_updates_on_startup;
    document[key_last_seen_version] = last_seen_version;
    document[key_record_sessions] = record_sessions;
    document[key_use_remaster] = use_remaster;
    document[key_remaster_root] = remaster_root.u8string();
    document[key_language] = language;
    document[key_installation] = active_installation ? active_installation->to_json() : Json();

    Json roots{Json::Object{}};

    for (const auto& [release, folder] : save_roots) {
        roots[release] = folder.u8string();
    }

    document[key_save_roots] = roots;
    Json set_up{Json::Object{}};

    for (const auto& [release, installation] : installations) {
        set_up[release] = installation.to_json();
    }

    document[key_installations] = set_up;
    return AtomicFile::write(path, document.dump(true), error);
}

std::filesystem::path LauncherSettings::save_root(const std::string& release_id) const {
    const auto found = save_roots.find(release_id);
    return found == save_roots.end() ? std::filesystem::path() : found->second;
}

}
