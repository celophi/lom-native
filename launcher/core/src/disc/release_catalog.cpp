#include "lom/launcher/disc/release_catalog.h"

#include "lom/launcher/common/atomic_file.h"
#include "lom/launcher/disc/disc_image.h"
#include "lom/launcher/common/json.h"
#include "common/text_case.h"

#include <set>

namespace lom::launcher {

namespace {

constexpr char key_schema[] = "schema";
constexpr char key_upstream[] = "upstream_revision";
constexpr char key_releases[] = "releases";
constexpr char key_id[] = "id";
constexpr char key_region[] = "region";
constexpr char key_serial[] = "serial";
constexpr char key_executable[] = "executable";
constexpr char key_display_name[] = "display_name";
constexpr char key_raw[] = "raw";
constexpr char key_sha1[] = "sha1";
constexpr char key_bytes[] = "bytes";
constexpr char key_layout[] = "layout";
constexpr char key_manifest[] = "manifest";
constexpr char key_runtime[] = "runtime";
constexpr char key_card_prefix[] = "card_prefix";
constexpr std::size_t sha1_hex_length = 40;

/**
 * @brief Read a string member.
 * @param object Parent object.
 * @param key Member name.
 * @return The string, or empty.
 */
std::string text_member(const Json& object, const char* key) {
    const Json* value = object.find(key);
    return value == nullptr ? std::string() : value->as_string();
}

/**
 * @brief Read a non-negative integer member.
 * @param object Parent object.
 * @param key Member name.
 * @return The integer, or zero.
 */
std::uint64_t size_member(const Json& object, const char* key) {
    const Json* value = object.find(key);
    const std::int64_t number = value == nullptr ? 0 : value->as_int(0);
    return number < 0 ? 0 : std::uint64_t(number);
}

/**
 * @brief Check an optional SHA-1 field.
 * @param text Field value.
 * @return True when empty or 40 lowercase hexadecimal characters.
 */
bool valid_optional_sha1(const std::string& text) {
    if (text.empty()) {
        return true;
    }

    if (text.size() != sha1_hex_length) {
        return false;
    }

    for (const char c : text) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }

    return true;
}

}

bool ReleaseCatalog::load(const std::filesystem::path& path, std::string* error) {
    const auto text = AtomicFile::read(path, error);

    if (!text) {
        return false;
    }

    std::string parse_error;
    const auto document = Json::parse(*text, &parse_error);

    if (!document) {
        if (error != nullptr) {
            *error = path.filename().u8string() + ": " + parse_error;
        }

        return false;
    }

    return from_json(*document, path.parent_path(), error);
}

bool ReleaseCatalog::from_json(const Json& document, const std::filesystem::path& base_directory, std::string* error) {
    releases_.clear();
    const Json* schema = document.find(key_schema);
    const Json* releases = document.find(key_releases);

    if (schema == nullptr || schema->as_int(0) != schema_version || releases == nullptr) {
        if (error != nullptr) {
            *error = "unsupported release catalog schema";
        }

        return false;
    }

    upstream_revision_ = text_member(document, key_upstream);
    std::set<std::string> ids;

    for (const Json& entry : releases->as_array()) {
        Release release;
        release.id = text_member(entry, key_id);
        release.region = text_member(entry, key_region);
        release.serial = text_member(entry, key_serial);
        release.executable = ascii_upper(text_member(entry, key_executable));
        release.display_name = text_member(entry, key_display_name);
        release.runtime = text_member(entry, key_runtime);
        release.card_prefix = text_member(entry, key_card_prefix);

        if (const Json* raw = entry.find(key_raw)) {
            release.raw_sha1 = text_member(*raw, key_sha1);
            release.raw_bytes = size_member(*raw, key_bytes);
            release.raw_layout = text_member(*raw, key_layout);
        }

        const std::string manifest = text_member(entry, key_manifest);

        if (!manifest.empty()) {
            release.manifest = base_directory / std::filesystem::u8path(manifest);
        }

        if (release.id.empty() || release.raw_sha1.empty() || !valid_optional_sha1(release.raw_sha1) || !ids.insert(release.id).second) {
            if (error != nullptr) {
                *error = "release catalog entry '" + release.id + "' is incomplete or duplicated";
            }

            releases_.clear();
            return false;
        }

        releases_.push_back(release);
    }

    return true;
}

const ReleaseCatalog::Release* ReleaseCatalog::identify(const std::string& layout, const std::string& sha1, std::uint64_t bytes) const {
    for (const Release& release : releases_) {
        if (layout == DiscImage::raw_single_track_layout && release.raw_layout == layout && release.raw_sha1 == sha1 &&
            (release.raw_bytes == 0 || release.raw_bytes == bytes)) {
            return &release;
        }
    }

    return nullptr;
}

const ReleaseCatalog::Release* ReleaseCatalog::by_executable(const std::string& executable) const {
    const std::string wanted = ascii_upper(executable);

    for (const Release& release : releases_) {
        if (!wanted.empty() && release.executable == wanted) {
            return &release;
        }
    }

    return nullptr;
}

const ReleaseCatalog::Release* ReleaseCatalog::by_card_filename(const std::string& filename) const {
    for (const Release& release : releases_) {
        if (!release.card_prefix.empty() && filename.compare(0, release.card_prefix.size(), release.card_prefix) == 0) {
            return &release;
        }
    }

    return nullptr;
}

const ReleaseCatalog::Release* ReleaseCatalog::by_id(const std::string& id) const {
    for (const Release& release : releases_) {
        if (release.id == id) {
            return &release;
        }
    }

    return nullptr;
}

const std::vector<ReleaseCatalog::Release>& ReleaseCatalog::releases() const {
    return releases_;
}

const std::string& ReleaseCatalog::upstream_revision() const {
    return upstream_revision_;
}

}
