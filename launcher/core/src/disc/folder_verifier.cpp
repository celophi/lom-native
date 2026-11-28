#include "lom/launcher/disc/folder_verifier.h"
#include "lom/launcher/common/files_provider.h"

#include "disc/file_digest.h"
#include "lom/launcher/disc/file_manifest.h"
#include "common/text_case.h"

#include <map>
#include <system_error>

namespace lom::launcher {

namespace {
constexpr const char* ignored_metadata_names[] = {".DS_Store", "Thumbs.db", "desktop.ini"};

struct FoundFile {
    std::string actual;
    std::string canonical;
    std::filesystem::path path;
    std::uint64_t bytes = 0;
    std::filesystem::file_time_type modified{};
};

struct Enumeration {
    FolderVerifier::Result::Status status = FolderVerifier::Result::Status::Verified;
    std::vector<FoundFile> files;
    std::vector<std::string> unsafe;
    std::vector<std::string> unreadable;
    std::string error;
};

/**
 * @brief Normalize a relative path to its canonical disc name.
 * @param actual Relative path with '/' separators.
 * @return Upper-case path without a terminal ";<digits>" version.
 */
std::string canonical_name(const std::string& actual) {
    std::string canonical = ascii_upper(actual);
    const std::size_t semicolon = canonical.rfind(';');

    if (semicolon != std::string::npos && semicolon + 1 < canonical.size() && canonical.find('/', semicolon) == std::string::npos) {
        bool digits = true;

        for (std::size_t i = semicolon + 1; i < canonical.size(); i++) {
            digits = digits && canonical[i] >= '0' && canonical[i] <= '9';
        }

        if (digits) {
            canonical.erase(semicolon);
        }
    }

    return canonical;
}

/**
 * @brief Check whether a path lies inside a directory.
 * @param path Canonical path.
 * @param root Canonical directory.
 * @return True when path is root or below it.
 */
bool is_within(const std::filesystem::path& path, const std::filesystem::path& root) {
    auto root_part = root.begin();
    auto path_part = path.begin();

    for (; root_part != root.end(); ++root_part, ++path_part) {
        if (path_part == path.end() || *path_part != *root_part) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Check whether a file name is ignored operating-system metadata.
 * @param name File name.
 * @return True when ignored.
 */
bool is_ignored_metadata(const std::string& name) {
    for (const char* ignored : ignored_metadata_names) {
        if (name == ignored) {
            return true;
        }
    }

    return false;
}

/**
 * @brief List every file under root without following directory links.
 * @param root Selected folder.
 * @return The enumeration.
 */
Enumeration enumerate(const std::filesystem::path& root) {
    Enumeration result;
    std::error_code code;
    const std::filesystem::path canonical_root = files_provider().weakly_canonical(root, code);
    const std::vector<FileEntry> entries = files_provider().list(root, true, code);

    if (code) {
        result.status = FolderVerifier::Result::Status::Unreadable;
        result.error = (entries.empty() ? "cannot read " : "cannot list ") + root.u8string() + ": " + code.message();
        return result;
    }

    for (const FileEntry& entry : entries) {
        const std::string actual = entry.path.lexically_relative(root).generic_u8string();

        if (entry.symlink) {
            const std::filesystem::path target = files_provider().weakly_canonical(entry.path, code);

            if (code || !is_within(target, canonical_root)) {
                result.unsafe.push_back(actual);
                continue;
            }
        }

        if (entry.directory) {
            continue;
        }

        if (is_ignored_metadata(entry.path.filename().u8string())) {
            continue;
        }

        if (!entry.regular) {
            result.unreadable.push_back(actual);
            continue;
        }

        if (result.files.size() >= FolderVerifier::max_files) {
            result.status = FolderVerifier::Result::Status::NotAGameFolder;
            result.error = "the folder contains too many files to be a game folder";
            return result;
        }

        FoundFile found;
        found.actual = actual;
        found.canonical = canonical_name(actual);
        found.path = entry.path;
        found.bytes = entry.size;
        found.modified = files_provider().last_write_time(entry.path, code);

        if (code) {
            result.unreadable.push_back(actual);
            continue;
        }

        result.files.push_back(found);
    }

    if (!result.unsafe.empty()) {
        result.status = FolderVerifier::Result::Status::UnsafeLink;
        result.error = "the folder contains links that point outside it";
    }

    return result;
}

/**
 * @brief Compare an enumeration with a manifest.
 * @param root Folder that was enumerated.
 * @param found Folder enumeration.
 * @param manifest Trusted manifest.
 * @param trusted Snapshot whose unchanged files need not be re-hashed, or null to hash every file.
 * @param progress Progress and cancellation.
 * @return The verification result, with a new snapshot when Verified.
 */
FolderVerifier::Result compare(const std::filesystem::path& root, const Enumeration& found, const FileManifest& manifest, const FileSnapshot* trusted,
                               const Progress& progress) {
    using Status = FolderVerifier::Result::Status;
    FolderVerifier::Result result;
    result.unreadable = found.unreadable;

    if (found.status != Status::Verified) {
        result.status = found.status;
        result.error = found.error;
        result.unexpected = found.unsafe;
        return result;
    }

    std::map<std::string, const FoundFile*> by_canonical;

    for (const FoundFile& file : found.files) {
        const auto inserted = by_canonical.emplace(file.canonical, &file);

        if (!inserted.second) {
            result.renamed.push_back(inserted.first->second->actual + ", " + file.actual + " -> " + file.canonical);
        }
    }

    if (!result.renamed.empty()) {
        result.status = Status::LayoutUnsupported;
        result.error = "several files have the same name after normalization";
        return result;
    }

    for (const FileManifest::Entry& entry : manifest.entries) {
        const auto file = by_canonical.find(entry.path);

        if (file == by_canonical.end()) {
            result.missing.push_back(entry.path);
        } else if (file->second->bytes != entry.bytes) {
            result.mismatched.push_back(entry.path);
        }
    }

    for (const auto& [canonical, file] : by_canonical) {
        if (manifest.find(canonical) == nullptr) {
            result.unexpected.push_back(file->actual);
        }
    }

    if (!result.missing.empty() || !result.unexpected.empty() || !result.mismatched.empty()) {
        result.status = Status::UnknownContents;
        return result;
    }

    const bool use_snapshot = trusted != nullptr && trusted->applies_to(root, manifest.aggregate_sha256());
    std::vector<const FileManifest::Entry*> to_hash;
    std::uint64_t total = 0;

    for (const FileManifest::Entry& entry : manifest.entries) {
        const FoundFile& file = *by_canonical.at(entry.path);

        if (!use_snapshot || !trusted->unchanged(entry.path, file.bytes, std::int64_t(file.modified.time_since_epoch().count()))) {
            to_hash.push_back(&entry);
            total += entry.bytes;
        }
    }

    {
        std::uint64_t done = 0;

        for (const FileManifest::Entry* hashed : to_hash) {
            const FileManifest::Entry& entry = *hashed;
            const FoundFile& file = *by_canonical.at(entry.path);
            std::string digest;
            const FileDigestStatus status = sha256_file(file.path, progress, Progress::Stage::CheckingFolder, done, total, digest);

            if (status == FileDigestStatus::Cancelled) {
                result.status = Status::Cancelled;
                return result;
            }

            if (status == FileDigestStatus::Unreadable) {
                result.unreadable.push_back(file.actual);
                continue;
            }

            std::error_code code;
            const std::uintmax_t bytes = files_provider().file_size(file.path, code);
            const auto modified = files_provider().last_write_time(file.path, code);

            if (code || bytes != file.bytes || modified != file.modified) {
                result.status = Status::ChangedDuringCheck;
                result.error = file.actual + " changed while it was being checked";
                return result;
            }

            if (digest != entry.sha256) {
                result.mismatched.push_back(entry.path);
            }
        }

        result.hashed_bytes = done;
    }

    if (!result.unreadable.empty()) {
        result.status = Status::Unreadable;
        return result;
    }

    if (!result.mismatched.empty()) {
        result.status = Status::UnknownContents;
        return result;
    }

    for (const auto& [canonical, file] : by_canonical) {
        if (file->actual != canonical) {
            result.renamed.push_back(file->actual + " -> " + canonical);
        }
    }

    if (!result.renamed.empty()) {
        result.status = Status::LayoutUnsupported;
        result.error = "the files match, but their names differ from the exact names the game opens";
        return result;
    }

    result.status = Status::Verified;
    result.snapshot.root = root;
    result.snapshot.manifest_sha256 = manifest.aggregate_sha256();

    for (const auto& [canonical, file] : by_canonical) {
        result.snapshot.entries.push_back(FileSnapshot::Entry{canonical, file->bytes, std::int64_t(file->modified.time_since_epoch().count())});
    }

    return result;
}

}

FolderVerifier::FolderVerifier(const ReleaseCatalog& catalog) : catalog_(catalog) {
}

FolderVerifier::Result FolderVerifier::verify(const std::filesystem::path& root, const Progress& progress) const {
    Result result;
    std::error_code code;

    if (!files_provider().is_directory(root, code)) {
        result.status = Result::Status::NotFound;
        result.error = "the folder is not available";
        return result;
    }

    const Enumeration found = enumerate(root);

    if (found.status != Result::Status::Verified) {
        result.status = found.status;
        result.error = found.error;
        result.unexpected = found.unsafe;
        result.unreadable = found.unreadable;
        return result;
    }

    const ReleaseCatalog::Release* release = nullptr;

    for (const FoundFile& file : found.files) {
        if (const ReleaseCatalog::Release* candidate = catalog_.by_executable(file.canonical)) {
            if (release != nullptr && release != candidate) {
                result.status = Result::Status::UnknownContents;
                result.error = "the folder contains executables from more than one edition";
                return result;
            }

            release = candidate;
        }
    }

    if (release == nullptr) {
        result.status = Result::Status::NotAGameFolder;
        result.error = "the folder does not contain a supported game executable at its top level";
        return result;
    }

    FileManifest manifest;
    std::string manifest_error;

    if (release->manifest.empty() || !manifest.load(release->manifest, &manifest_error) || manifest.release_id != release->id ||
        manifest.parent_raw_sha1 != release->raw_sha1) {
        result.status = Result::Status::RegionUnavailable;
        result.release = release;
        result.error = release->manifest.empty() ? "no trusted file list is available for this edition yet"
                       : manifest_error.empty()  ? "the file list doesn't match this edition"
                                                 : manifest_error;
        return result;
    }

    result = compare(root, found, manifest, nullptr, progress);
    result.release = release;
    return result;
}

/**
 * @brief Compare a folder with one manifest, re-hashing only what changed since a trusted snapshot.
 * @param root Folder containing the extracted disc tree.
 * @param manifest Trusted manifest.
 * @param trusted Snapshot from the last full verification of this folder, or null to hash every file.
 * @param progress CheckingFolder progress in bytes and cancellation.
 * @return The verification result; release is left null.
 */
FolderVerifier::Result FolderVerifier::check(const std::filesystem::path& root, const FileManifest& manifest, const FileSnapshot* trusted,
                                             const Progress& progress) {
    std::error_code code;

    if (!files_provider().is_directory(root, code)) {
        Result result;
        result.status = Result::Status::NotFound;
        result.error = "the folder is not available";
        return result;
    }

    return compare(root, enumerate(root), manifest, trusted, progress);
}

}
