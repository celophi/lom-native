#include "lom/launcher/disc/file_manifest.h"

#include "lom/launcher/common/atomic_file.h"
#include "lom/launcher/common/json.h"
#include "lom/launcher/common/sha256.h"

#include <algorithm>
#include <set>

namespace lom::launcher {

namespace {

constexpr char key_schema[] = "schema";
constexpr char key_release[] = "release";
constexpr char key_parent_raw_sha1[] = "parent_raw_sha1";
constexpr char key_upstream_revision[] = "upstream_revision";
constexpr char key_generator[] = "generator";
constexpr char key_files[] = "files";
constexpr char key_path[] = "path";
constexpr char key_bytes[] = "bytes";
constexpr char key_sha256[] = "sha256";
constexpr char key_aggregate[] = "aggregate_sha256";
constexpr std::size_t sha256_hex_length = 64;

/**
 * @brief Check a lowercase hexadecimal digest.
 * @param text Candidate digest.
 * @param length Expected characters.
 * @return True when valid.
 */
bool is_hex_digest(const std::string& text, std::size_t length) {
    return text.size() == length && std::all_of(text.begin(), text.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

/**
 * @brief Set an error message when requested.
 * @param error Destination; may be null.
 * @param message Message.
 * @return Always false.
 */
bool fail(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }

    return false;
}

}

bool FileManifest::load(const std::filesystem::path& path, std::string* error) {
    std::string read_error;
    const auto text = AtomicFile::read(path, &read_error);

    if (!text) {
        return fail(error, read_error);
    }

    std::string parse_error;
    const auto document = Json::parse(*text, &parse_error);

    if (!document) {
        return fail(error, path.filename().u8string() + ": " + parse_error);
    }

    return from_json(*document, error);
}

bool FileManifest::from_json(const Json& document, std::string* error) {
    entries.clear();
    const Json* schema = document.find(key_schema);

    if (schema == nullptr || schema->as_int(0) != schema_version) {
        return fail(error, "unsupported manifest schema");
    }

    const Json* release = document.find(key_release);
    const Json* parent = document.find(key_parent_raw_sha1);
    const Json* files = document.find(key_files);

    if (release == nullptr || release->as_string().empty() || parent == nullptr || files == nullptr || files->type() != Json::Type::Array) {
        return fail(error, "manifest is missing its release, parent identity or file list");
    }

    release_id = release->as_string();
    parent_raw_sha1 = parent->as_string();

    if (const Json* revision = document.find(key_upstream_revision)) {
        upstream_revision = revision->as_string();
    }

    if (const Json* tool = document.find(key_generator)) {
        generator = tool->as_string();
    }

    std::set<std::string> seen;

    for (const Json& file : files->as_array()) {
        const Json* path = file.find(key_path);
        const Json* bytes = file.find(key_bytes);
        const Json* sha256 = file.find(key_sha256);

        if (path == nullptr || bytes == nullptr || sha256 == nullptr || path->as_string().empty() || bytes->as_int(-1) < 0 ||
            !is_hex_digest(sha256->as_string(), sha256_hex_length)) {
            return fail(error, "manifest has an incomplete file entry");
        }

        if (!seen.insert(path->as_string()).second) {
            return fail(error, "manifest lists " + path->as_string() + " twice");
        }

        entries.push_back(Entry{path->as_string(), std::uint64_t(bytes->as_int(0)), sha256->as_string()});
    }

    if (entries.empty()) {
        return fail(error, "manifest has no files");
    }

    sort_entries();

    if (const Json* aggregate = document.find(key_aggregate)) {
        if (aggregate->as_string() != aggregate_sha256()) {
            return fail(error, "manifest aggregate identity does not match its entries");
        }
    }

    return true;
}

Json FileManifest::to_json() const {
    std::vector<Entry> sorted = entries;
    std::sort(sorted.begin(), sorted.end(), [](const Entry& a, const Entry& b) {
        return a.path < b.path;
    });

    Json files{Json::Array{}};

    for (const Entry& entry : sorted) {
        Json item;
        item[key_path] = entry.path;
        item[key_bytes] = std::int64_t(entry.bytes);
        item[key_sha256] = entry.sha256;
        files.push_back(item);
    }

    Json document;
    document[key_schema] = schema_version;
    document[key_release] = release_id;
    document[key_parent_raw_sha1] = parent_raw_sha1;
    document[key_upstream_revision] = upstream_revision;
    document[key_generator] = generator;
    document[key_files] = files;
    document[key_aggregate] = aggregate_sha256();
    return document;
}

std::string FileManifest::aggregate_sha256() const {
    std::vector<const Entry*> sorted;

    for (const Entry& entry : entries) {
        sorted.push_back(&entry);
    }

    std::sort(sorted.begin(), sorted.end(), [](const Entry* a, const Entry* b) {
        return a->path < b->path;
    });

    Sha256 hash;

    for (const Entry* entry : sorted) {
        const std::string record = entry->path + '\0' + std::to_string(entry->bytes) + '\0' + entry->sha256 + '\n';
        hash.update(record.data(), record.size());
    }

    return hash.hex_digest();
}

void FileManifest::sort_entries() {
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        return a.path < b.path;
    });
}

const FileManifest::Entry* FileManifest::find(const std::string& path) const {
    const auto found = std::lower_bound(entries.begin(), entries.end(), path, [](const Entry& entry, const std::string& wanted) {
        return entry.path < wanted;
    });

    if (found != entries.end() && found->path == path) {
        return &*found;
    }

    return nullptr;
}

std::uint64_t FileManifest::total_bytes() const {
    std::uint64_t total = 0;

    for (const Entry& entry : entries) {
        total += entry.bytes;
    }

    return total;
}

}
