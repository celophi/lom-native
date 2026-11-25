#include "lom/launcher/disc/file_snapshot.h"

#include "lom/launcher/common/atomic_file.h"
#include "lom/launcher/disc/file_manifest.h"
#include "lom/launcher/common/json.h"

#include <algorithm>
#include <system_error>

namespace lom::launcher {

namespace {

constexpr char key_schema[] = "schema";
constexpr char key_root[] = "root";
constexpr char key_manifest[] = "manifest_sha256";
constexpr char key_files[] = "files";
constexpr char key_path[] = "path";
constexpr char key_bytes[] = "bytes";
constexpr char key_modified[] = "modified_ticks";

}

std::optional<FileSnapshot> FileSnapshot::capture(const std::filesystem::path& root, const FileManifest& manifest) {
    FileSnapshot snapshot;
    snapshot.root = root;
    snapshot.manifest_sha256 = manifest.aggregate_sha256();

    for (const FileManifest::Entry& entry : manifest.entries) {
        const std::filesystem::path file = root / std::filesystem::u8path(entry.path);
        std::error_code code;
        const std::uintmax_t bytes = std::filesystem::file_size(file, code);
        const auto modified = std::filesystem::last_write_time(file, code);

        if (code || bytes != entry.bytes) {
            return std::nullopt;
        }

        snapshot.entries.push_back(Entry{entry.path, bytes, std::int64_t(modified.time_since_epoch().count())});
    }

    std::sort(snapshot.entries.begin(), snapshot.entries.end(), [](const Entry& a, const Entry& b) {
        return a.path < b.path;
    });
    return snapshot;
}

std::optional<FileSnapshot> FileSnapshot::load(const std::filesystem::path& path) {
    const auto text = AtomicFile::read(path, nullptr);

    if (!text) {
        return std::nullopt;
    }

    const auto document = Json::parse(*text, nullptr);
    const Json* schema = document ? document->find(key_schema) : nullptr;
    const Json* root = document ? document->find(key_root) : nullptr;
    const Json* manifest = document ? document->find(key_manifest) : nullptr;
    const Json* files = document ? document->find(key_files) : nullptr;

    if (schema == nullptr || schema->as_int(0) != schema_version || root == nullptr || manifest == nullptr || files == nullptr) {
        return std::nullopt;
    }

    FileSnapshot snapshot;
    snapshot.root = std::filesystem::u8path(root->as_string());
    snapshot.manifest_sha256 = manifest->as_string();

    for (const Json& file : files->as_array()) {
        const Json* file_path = file.find(key_path);
        const Json* bytes = file.find(key_bytes);
        const Json* modified = file.find(key_modified);

        if (file_path == nullptr || bytes == nullptr || modified == nullptr || bytes->as_int(-1) < 0) {
            return std::nullopt;
        }

        snapshot.entries.push_back(Entry{file_path->as_string(), std::uint64_t(bytes->as_int(0)), modified->as_int(0)});
    }

    std::sort(snapshot.entries.begin(), snapshot.entries.end(), [](const Entry& a, const Entry& b) {
        return a.path < b.path;
    });
    return snapshot;
}

bool FileSnapshot::save(const std::filesystem::path& path, std::string* error) const {
    Json files{Json::Array{}};

    for (const Entry& entry : entries) {
        Json item;
        item[key_path] = entry.path;
        item[key_bytes] = std::int64_t(entry.bytes);
        item[key_modified] = entry.modified_ticks;
        files.push_back(item);
    }

    Json document;
    document[key_schema] = schema_version;
    document[key_root] = root.u8string();
    document[key_manifest] = manifest_sha256;
    document[key_files] = files;
    return AtomicFile::write(path, document.dump(false), error);
}

bool FileSnapshot::applies_to(const std::filesystem::path& folder, const std::string& manifest_identity) const {
    return root == folder && manifest_sha256 == manifest_identity;
}

bool FileSnapshot::unchanged(const std::string& path, std::uint64_t bytes, std::int64_t modified_ticks) const {
    const auto found = std::lower_bound(entries.begin(), entries.end(), path, [](const Entry& entry, const std::string& wanted) {
        return entry.path < wanted;
    });
    return found != entries.end() && found->path == path && found->bytes == bytes && found->modified_ticks == modified_ticks;
}

}
