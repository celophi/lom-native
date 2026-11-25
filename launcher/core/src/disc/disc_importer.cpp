#include "lom/launcher/disc/disc_importer.h"

#include "disc/file_digest.h"
#include "lom/launcher/common/atomic_file.h"
#include "lom/launcher/disc/disc_image.h"
#include "lom/launcher/disc/file_manifest.h"
#include "lom/launcher/disc/iso9660_reader.h"
#include "lom/launcher/common/json.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <system_error>

namespace lom::launcher {

namespace {
constexpr std::uint64_t space_margin_bytes = 64ull * 1024 * 1024;
constexpr char receipt_key_schema[] = "schema";
constexpr char receipt_key_release[] = "release";
constexpr char receipt_key_source_sha1[] = "source_sha1";
constexpr char receipt_key_source_layout[] = "source_layout";
constexpr char receipt_key_manifest[] = "manifest_sha256";
constexpr char receipt_key_extractor[] = "extractor_version";
constexpr char receipt_key_installation[] = "installation_id";
constexpr char receipt_key_created[] = "created_unix_seconds";
constexpr char receipt_key_verification[] = "verification";
constexpr char verification_raw_identity[] = "raw-identity";
constexpr char verification_file_contents[] = "file-contents";

constexpr std::uint32_t installation_suffix_mask = 0xFFFFFF;

/**
 * @brief Create a unique, sortable installation identifier.
 * @param seconds Current Unix time.
 * @param random Random source for the suffix.
 * @return Identifier made of the UTC time in seconds and a random suffix.
 */
std::string new_installation_id(std::int64_t seconds, RandomSource& random) {
    char suffix[8];
    std::snprintf(suffix, sizeof(suffix), "%06x", unsigned(random.next() & installation_suffix_mask));
    return std::to_string(seconds) + "-" + suffix;
}

/**
 * @brief Map an image open failure to an identification status.
 * @param status DiscImage status.
 * @return Identification status.
 */
DiscImporter::Identification::Status open_failure(DiscImage::Status status) {
    using Status = DiscImporter::Identification::Status;

    switch (status) {
    case DiscImage::Status::NotFound:
        return Status::NotFound;

    case DiscImage::Status::Unreadable:
        return Status::Unreadable;

    case DiscImage::Status::MissingTrackFile:
        return Status::MissingTrackFile;

    case DiscImage::Status::UnsupportedLayout:
        return Status::UnsupportedLayout;

    case DiscImage::Status::UnsupportedFormat:
        return Status::UnsupportedFormat;

    case DiscImage::Status::Malformed:
    case DiscImage::Status::Ok:
        break;
    }

    return Status::Malformed;
}

/**
 * @brief Remove a directory tree, ignoring errors.
 * @param path Directory to remove.
 */
void remove_quietly(const std::filesystem::path& path) {
    std::error_code code;
    std::filesystem::remove_all(path, code);
}

/**
 * @brief Compare a disc's file list with a manifest by path and length.
 * @param reader Disc file tree.
 * @param manifest Trusted manifest.
 * @return Paths that are extra, missing or of a different length; empty when the lists match.
 */
std::vector<std::string> file_list_differences(const Iso9660Reader& reader, const FileManifest& manifest) {
    std::vector<std::string> differences;
    std::set<std::string> disc_paths;

    for (const Iso9660Reader::File& file : reader.files()) {
        disc_paths.insert(file.path);
        const FileManifest::Entry* expected = manifest.find(file.path);

        if (expected == nullptr || expected->bytes != file.bytes) {
            differences.push_back(file.path);
        }
    }

    for (const FileManifest::Entry& entry : manifest.entries) {
        if (disc_paths.count(entry.path) == 0) {
            differences.push_back(entry.path);
        }
    }

    return differences;
}

}

DiscImporter::DiscImporter(const ReleaseCatalog& catalog, const Clock& clock, RandomSource& random) : catalog_(catalog), clock_(clock), random_(random) {
}

DiscImporter::Identification DiscImporter::identify(const std::filesystem::path& selected, const Progress& progress) const {
    Identification result;
    result.selected = selected;
    DiscImage image;
    const DiscImage::Status opened = image.open(selected);

    if (opened != DiscImage::Status::Ok) {
        result.status = open_failure(opened);
        result.error = image.error();
        return result;
    }

    result.layout = image.layout();
    result.bytes = image.data_bytes();

    Iso9660Reader reader;
    const bool tree_readable = reader.read(image);

    if (tree_readable) {
        result.recognized = catalog_.by_executable(reader.boot_executable(image));
    }

    if (!image.hash_identity(progress, result.sha1)) {
        result.status = progress.cancelled() ? Identification::Status::Cancelled : Identification::Status::Unreadable;
        result.error = image.error();
        return result;
    }

    if (image.representation() == DiscImage::Representation::RawSectors) {
        result.release = catalog_.identify(result.layout, result.sha1, result.bytes);

        if (result.release != nullptr) {
            result.status = Identification::Status::Supported;
            return result;
        }

        if (!tree_readable) {
            result.status = Identification::Status::UnknownIdentity;
            return result;
        }
    }

    if (!tree_readable) {
        result.status = Identification::Status::Malformed;
        result.error = reader.error();
        return result;
    }

    if (result.recognized == nullptr) {
        result.status = Identification::Status::UnknownIdentity;
        return result;
    }

    FileManifest manifest;
    std::string manifest_error;

    if (result.recognized->manifest.empty() || !manifest.load(result.recognized->manifest, &manifest_error) || manifest.release_id != result.recognized->id ||
        manifest.parent_raw_sha1 != result.recognized->raw_sha1) {
        result.status = Identification::Status::NoTrustedFileList;
        result.error = manifest_error;
        return result;
    }

    result.problems = file_list_differences(reader, manifest);

    if (!result.problems.empty()) {
        result.status = Identification::Status::UnknownIdentity;
        result.error = "the disc's files differ from the supported edition's file list";
        return result;
    }

    result.status = Identification::Status::Supported;
    result.basis = Identification::Basis::FileContents;
    result.release = result.recognized;
    return result;
}

DiscImporter::InstallResult DiscImporter::install(const Identification& identification, const FileManifest& manifest, const std::filesystem::path& games_root,
                                                  const Progress& progress) const {
    InstallResult result;
    const ReleaseCatalog::Release* release = identification.release;

    if (identification.status != Identification::Status::Supported || release == nullptr || manifest.release_id != release->id ||
        manifest.parent_raw_sha1 != release->raw_sha1) {
        result.status = InstallResult::Status::NoManifest;
        result.error = "no trusted file manifest is available for this edition";
        return result;
    }

    std::error_code code;
    std::filesystem::create_directories(games_root, code);

    if (code) {
        result.status = InstallResult::Status::DestinationUnavailable;
        result.error = "cannot create " + games_root.u8string() + ": " + code.message();
        return result;
    }

    result.required_bytes = required_space(manifest);
    const std::filesystem::space_info space = std::filesystem::space(games_root, code);

    if (!code && space.available < result.required_bytes) {
        result.status = InstallResult::Status::InsufficientSpace;
        result.error = "not enough free space";
        return result;
    }

    DiscImage image;

    if (image.open(identification.selected) != DiscImage::Status::Ok || image.data_bytes() != identification.bytes) {
        result.status = InstallResult::Status::SourceChanged;
        result.error = "the disc image changed after it was checked";
        return result;
    }

    Iso9660Reader reader;

    if (!reader.read(image)) {
        result.status = InstallResult::Status::ExtractionFailed;
        result.error = reader.error();
        return result;
    }

    result.problems = file_list_differences(reader, manifest);

    if (!result.problems.empty()) {
        result.status = InstallResult::Status::ExtractionFailed;
        result.error = "the disc's file list does not match the trusted manifest";
        return result;
    }

    const std::string installation_id = new_installation_id(clock_.now_unix_seconds(), random_);
    const std::filesystem::path staging = games_root / staging_directory / installation_id;
    const std::filesystem::path staging_data = staging / data_directory;
    std::filesystem::create_directories(staging_data, code);

    if (code) {
        result.status = InstallResult::Status::DestinationUnavailable;
        result.error = "cannot create " + staging_data.u8string() + ": " + code.message();
        return result;
    }

    const std::uint64_t total = manifest.total_bytes();
    std::uint64_t written = 0;
    std::uint8_t sector[DiscImage::user_sector_bytes];

    for (const Iso9660Reader::File& file : reader.files()) {
        const std::filesystem::path output = staging_data / std::filesystem::u8path(file.path);
        std::filesystem::create_directories(output.parent_path(), code);
        std::ofstream out(output, std::ios::binary | std::ios::trunc);

        if (code || !out) {
            remove_quietly(staging);
            result.status = InstallResult::Status::DestinationUnavailable;
            result.error = "cannot write " + output.u8string();
            return result;
        }

        std::uint32_t remaining = file.bytes;
        std::uint32_t lba = file.lba;

        while (remaining > 0) {
            if (progress.cancelled()) {
                out.close();
                remove_quietly(staging);
                result.status = InstallResult::Status::Cancelled;
                return result;
            }

            DiscImage::SectorKind kind = DiscImage::SectorKind::Mode2Form1;

            if (!image.read_user_data(lba, sector, &kind) || kind == DiscImage::SectorKind::Mode2Form2) {
                out.close();
                remove_quietly(staging);
                result.status = InstallResult::Status::ExtractionFailed;
                result.error = file.path + ": " + (kind == DiscImage::SectorKind::Mode2Form2 ? "unsupported Form 2 sector" : image.error());
                return result;
            }

            const std::uint32_t take = remaining < DiscImage::user_sector_bytes ? remaining : DiscImage::user_sector_bytes;
            out.write(reinterpret_cast<const char*>(sector), take);
            remaining -= take;
            written += take;
            lba++;
            progress.report(Progress::Stage::Extracting, written, total);
        }

        out.close();

        if (!out) {
            remove_quietly(staging);
            result.status = InstallResult::Status::DestinationUnavailable;
            result.error = "cannot write " + output.u8string() + " (the disk may be full)";
            return result;
        }
    }

    if (!image.unchanged_since_open()) {
        remove_quietly(staging);
        result.status = InstallResult::Status::SourceChanged;
        result.error = "the disc image changed during extraction";
        return result;
    }

    std::uint64_t verified = 0;

    for (const FileManifest::Entry& entry : manifest.entries) {
        std::string digest;
        const FileDigestStatus status =
            sha256_file(staging_data / std::filesystem::u8path(entry.path), progress, Progress::Stage::VerifyingFiles, verified, total, digest);

        if (status == FileDigestStatus::Cancelled) {
            remove_quietly(staging);
            result.status = InstallResult::Status::Cancelled;
            return result;
        }

        if (status != FileDigestStatus::Ok || digest != entry.sha256) {
            result.problems.push_back(entry.path);
        }
    }

    if (!result.problems.empty()) {
        remove_quietly(staging);
        result.status = InstallResult::Status::VerificationFailed;
        result.error = "extracted files do not match the trusted manifest";
        return result;
    }

    Json receipt;
    receipt[receipt_key_schema] = extractor_version;
    receipt[receipt_key_release] = release->id;
    receipt[receipt_key_source_sha1] = identification.sha1;
    receipt[receipt_key_source_layout] = identification.layout;
    receipt[receipt_key_manifest] = manifest.aggregate_sha256();
    receipt[receipt_key_extractor] = extractor_version;
    receipt[receipt_key_verification] = identification.basis == Identification::Basis::FileContents ? verification_file_contents : verification_raw_identity;
    receipt[receipt_key_installation] = installation_id;
    receipt[receipt_key_created] = clock_.now_unix_seconds();

    std::string write_error;

    if (!AtomicFile::write(staging / receipt_file, receipt.dump(true), &write_error)) {
        remove_quietly(staging);
        result.status = InstallResult::Status::DestinationUnavailable;
        result.error = write_error;
        return result;
    }

    const std::filesystem::path release_directory = games_root / release->id / release->raw_sha1;
    const std::filesystem::path final_directory = release_directory / installation_id;
    std::filesystem::create_directories(release_directory, code);

    if (!code) {
        std::filesystem::rename(staging, final_directory, code);
    }

    if (code) {
        remove_quietly(staging);
        result.status = InstallResult::Status::DestinationUnavailable;
        result.error = "cannot publish the installation: " + code.message();
        return result;
    }

    std::filesystem::remove(games_root / staging_directory, code);
    result.status = InstallResult::Status::Installed;
    result.installation.kind = Installation::Kind::Managed;
    result.installation.data_path = final_directory / data_directory;
    result.installation.release_id = release->id;
    result.installation.manifest_sha256 = manifest.aggregate_sha256();
    result.installation.source_sha1 = identification.sha1;
    result.installation.installation_id = installation_id;
    result.snapshot = FileSnapshot::capture(result.installation.data_path, manifest);
    return result;
}

void DiscImporter::remove_stale_staging(const std::filesystem::path& games_root) {
    remove_quietly(games_root / staging_directory);
}

std::uint64_t DiscImporter::required_space(const FileManifest& manifest) {
    return manifest.total_bytes() + space_margin_bytes;
}

}
