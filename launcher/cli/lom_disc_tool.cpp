#include "lom/launcher/disc/disc_image.h"
#include "lom/launcher/disc/disc_importer.h"
#include "lom/launcher/disc/file_manifest.h"
#include "lom/launcher/disc/file_snapshot.h"
#include "lom/launcher/disc/folder_verifier.h"
#include "lom/launcher/disc/iso9660_reader.h"
#include "lom/launcher/common/json.h"
#include "lom/launcher/saves/lom_save.h"
#include "lom/launcher/saves/memory_card.h"
#include "lom/launcher/disc/release_catalog.h"
#include "lom/launcher/common/sha1.h"
#include "lom/launcher/common/sha256.h"
#include "lom/launcher/common/atomic_file.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

using namespace lom::launcher;

constexpr char usage[] = "usage:\n"
                         "  lom-disc-tool identify <image> --catalog <supported-discs.json>\n"
                         "  lom-disc-tool manifest <image> --catalog <json> --release <id> --out <manifest.json> [--executable-sha1 <sha1>]\n"
                         "  lom-disc-tool project-iso <raw-image> <out.iso>\n"
                         "  lom-disc-tool verify-folder <folder> --catalog <json>\n"
                         "  lom-disc-tool check-folder <folder> --catalog <json> --release <id> --snapshot <file>\n"
                         "  lom-disc-tool install <image> --catalog <json> --games-root <dir>\n"
                         "  lom-disc-tool card <card.mcr>\n";
constexpr char manifest_generator[] = "lom-disc-tool manifest v1";
constexpr int exit_failure = 1;
constexpr int exit_usage = 2;

/**
 * @brief Find the value following an option.
 * @param argc Argument count.
 * @param argv Arguments.
 * @param name Option name.
 * @return The value, or empty when absent.
 */
std::string option(int argc, char** argv, const char* name) {
    for (int i = 0; i + 1 < argc; i++) {
        if (std::strcmp(argv[i], name) == 0) {
            return argv[i + 1];
        }
    }

    return {};
}

/**
 * @brief Load the catalog named by --catalog.
 * @param argc Argument count.
 * @param argv Arguments.
 * @param catalog Receives the catalog.
 * @return True on success; prints the error otherwise.
 */
bool load_catalog(int argc, char** argv, ReleaseCatalog& catalog) {
    const std::string path = option(argc, argv, "--catalog");
    std::string error;

    if (path.empty() || !catalog.load(std::filesystem::u8path(path), &error)) {
        std::fprintf(stderr, "lom-disc-tool: cannot load the catalog: %s\n", path.empty() ? "--catalog is required" : error.c_str());
        return false;
    }

    return true;
}

/**
 * @brief Print an image identification.
 * @param argc Argument count.
 * @param argv Arguments.
 * @return Process exit status: zero only for a supported image.
 */
int identify(int argc, char** argv) {
    ReleaseCatalog catalog;

    if (!load_catalog(argc, argv, catalog)) {
        return exit_failure;
    }

    const DiscImporter importer(catalog);
    const DiscImporter::Identification result = importer.identify(std::filesystem::u8path(argv[2]), Progress());
    std::printf("status %d\nlayout %s\nbytes %llu\nsha1 %s\nrecognized %s\nrelease %s\n", int(result.status), result.layout.c_str(),
                static_cast<unsigned long long>(result.bytes), result.sha1.c_str(), result.recognized ? result.recognized->id.c_str() : "-",
                result.release ? result.release->id.c_str() : "-");

    if (result.status == DiscImporter::Identification::Status::Supported) {
        std::printf("basis %s\n", result.basis == DiscImporter::Identification::Basis::FileContents ? "file-contents" : "raw-identity");
    }

    for (const std::string& path : result.problems) {
        std::printf("differs %s\n", path.c_str());
    }

    if (!result.error.empty()) {
        std::printf("error %s\n", result.error.c_str());
    }

    return result.status == DiscImporter::Identification::Status::Supported ? 0 : exit_failure;
}

/**
 * @brief Hash the boot executable on a disc image.
 * @param image The image.
 * @param reader Its file tree.
 * @return The executable's SHA-1, or empty when it can't be read.
 */
std::string executable_sha1(DiscImage& image, const Iso9660Reader& reader) {
    const std::string name = reader.boot_executable(image);

    for (const Iso9660Reader::File& file : reader.files()) {
        std::vector<std::uint8_t> bytes;

        if (file.path == name && Iso9660Reader::read_file(image, file, bytes)) {
            Sha1 hash;
            hash.update(bytes.data(), bytes.size());
            return hash.hex_digest();
        }
    }

    return {};
}

/**
 * @brief Generate a trusted manifest from an image whose raw identity the
 *        catalog accepts. With --executable-sha1, the image can instead have
 *        2048-byte sectors, when its boot executable has that SHA-1 (the
 *        real disc's); the manifest then says so in its
 *        generator, since the raw image's hash couldn't be checked.
 * @param argc Argument count.
 * @param argv Arguments.
 * @return Process exit status.
 */
int manifest(int argc, char** argv) {
    ReleaseCatalog catalog;

    if (!load_catalog(argc, argv, catalog)) {
        return exit_failure;
    }

    const std::string release_id = option(argc, argv, "--release");
    const std::string out = option(argc, argv, "--out");
    const std::string expected_executable = option(argc, argv, "--executable-sha1");
    const DiscImporter importer(catalog);
    const DiscImporter::Identification identification = importer.identify(std::filesystem::u8path(argv[2]), Progress());
    const ReleaseCatalog::Release* release = catalog.by_id(release_id);
    const bool raw = identification.status == DiscImporter::Identification::Status::Supported && identification.release->id == release_id &&
                     identification.layout == DiscImage::raw_single_track_layout;
    const bool cooked = !expected_executable.empty() && release != nullptr && identification.layout != DiscImage::raw_single_track_layout &&
                        identification.recognized == release;

    if ((!raw && !cooked) || out.empty()) {
        std::fprintf(stderr,
                     "lom-disc-tool: manifests are generated only from a raw image matching the upstream identity of '%s', "
                     "or from a 2048-byte image of it with --executable-sha1\n",
                     release_id.c_str());
        return exit_failure;
    }

    DiscImage image;
    Iso9660Reader reader;

    if (image.open(std::filesystem::u8path(argv[2])) != DiscImage::Status::Ok || !reader.read(image)) {
        std::fprintf(stderr, "lom-disc-tool: %s\n", reader.error().c_str());
        return exit_failure;
    }

    if (cooked && !raw && executable_sha1(image, reader) != expected_executable) {
        std::fprintf(stderr, "lom-disc-tool: the image's boot executable doesn't have SHA-1 %s\n", expected_executable.c_str());
        return exit_failure;
    }

    FileManifest result;
    result.release_id = release_id;
    result.parent_raw_sha1 = raw ? identification.sha1 : release->raw_sha1;
    result.upstream_revision = catalog.upstream_revision();
    result.generator =
        raw ? std::string(manifest_generator) : std::string(manifest_generator) + " from a 2048-byte image whose executable has SHA-1 " + expected_executable;
    std::vector<std::uint8_t> bytes;

    for (const Iso9660Reader::File& file : reader.files()) {
        if (!Iso9660Reader::read_file(image, file, bytes)) {
            std::fprintf(stderr, "lom-disc-tool: cannot read %s as Form 1 data\n", file.path.c_str());
            return exit_failure;
        }

        Sha256 hash;
        hash.update(bytes.data(), bytes.size());
        result.entries.push_back(FileManifest::Entry{file.path, file.bytes, hash.hex_digest()});
    }

    result.sort_entries();
    std::string error;

    if (!AtomicFile::write(std::filesystem::u8path(out), result.to_json().dump(true), &error)) {
        std::fprintf(stderr, "lom-disc-tool: %s\n", error.c_str());
        return exit_failure;
    }

    std::printf("files %zu\nbytes %llu\naggregate_sha256 %s\n", result.entries.size(), static_cast<unsigned long long>(result.total_bytes()),
                result.aggregate_sha256().c_str());
    return 0;
}

/**
 * @brief Write the 2048-byte user-data projection of a raw image and print its identity.
 * @param argc Argument count.
 * @param argv Arguments.
 * @return Process exit status.
 */
int project_iso(int argc, char** argv) {
    if (argc < 4) {
        std::fputs(usage, stderr);
        return exit_usage;
    }

    DiscImage image;

    if (image.open(std::filesystem::u8path(argv[2])) != DiscImage::Status::Ok || image.representation() != DiscImage::Representation::RawSectors) {
        std::fprintf(stderr, "lom-disc-tool: %s is not a raw image\n", argv[2]);
        return exit_failure;
    }

    std::ofstream out(std::filesystem::u8path(argv[3]), std::ios::binary | std::ios::trunc);
    Sha1 sha1;
    std::uint8_t sector[DiscImage::user_sector_bytes];
    std::uint64_t bytes = 0;
    std::uint32_t form2 = 0;

    for (std::uint32_t lba = 0; lba < image.sector_count(); lba++) {
        DiscImage::SectorKind kind;

        if (!image.read_user_data(lba, sector, &kind)) {
            std::fprintf(stderr, "lom-disc-tool: %s\n", image.error().c_str());
            return exit_failure;
        }

        form2 += kind == DiscImage::SectorKind::Mode2Form2 ? 1 : 0;
        out.write(reinterpret_cast<const char*>(sector), sizeof(sector));
        sha1.update(sector, sizeof(sector));
        bytes += sizeof(sector);
    }

    out.close();

    if (!out) {
        std::fprintf(stderr, "lom-disc-tool: cannot write %s\n", argv[3]);
        return exit_failure;
    }

    std::printf("derivation mode2-user-data-2048-v1\nsectors %u\nform2_sectors %u\nbytes %llu\nsha1 %s\n", image.sector_count(), form2,
                static_cast<unsigned long long>(bytes), sha1.hex_digest().c_str());
    return 0;
}

/**
 * @brief Verify an extracted folder.
 * @param argc Argument count.
 * @param argv Arguments.
 * @return Process exit status: zero only when verified.
 */
int verify_folder(int argc, char** argv) {
    ReleaseCatalog catalog;

    if (!load_catalog(argc, argv, catalog)) {
        return exit_failure;
    }

    const FolderVerifier verifier(catalog);
    const FolderVerifier::Result result = verifier.verify(std::filesystem::u8path(argv[2]), Progress());
    std::printf("status %d\nrelease %s\nmissing %zu\nunexpected %zu\nmismatched %zu\nunreadable %zu\nrenamed %zu\n", int(result.status),
                result.release ? result.release->id.c_str() : "-", result.missing.size(), result.unexpected.size(), result.mismatched.size(),
                result.unreadable.size(), result.renamed.size());

    for (const auto* list : {&result.missing, &result.unexpected, &result.mismatched, &result.unreadable, &result.renamed}) {
        for (std::size_t i = 0; i < list->size() && i < 5; i++) {
            std::printf("  %s\n", (*list)[i].c_str());
        }
    }

    if (!result.error.empty()) {
        std::printf("error %s\n", result.error.c_str());
    }

    return result.status == FolderVerifier::Result::Status::Verified ? 0 : exit_failure;
}

/**
 * @brief Check a folder against one release, re-hashing only what changed since a saved snapshot, then update the snapshot.
 * @param argc Argument count.
 * @param argv Arguments.
 * @return Process exit status: zero only when verified.
 */
int check_folder(int argc, char** argv) {
    ReleaseCatalog catalog;

    if (!load_catalog(argc, argv, catalog)) {
        return exit_failure;
    }

    const ReleaseCatalog::Release* release = catalog.by_id(option(argc, argv, "--release"));
    const std::string snapshot_path = option(argc, argv, "--snapshot");
    FileManifest manifest;
    std::string error;

    if (release == nullptr || snapshot_path.empty() || !manifest.load(release->manifest, &error)) {
        std::fprintf(stderr, "lom-disc-tool: --release with a manifest and --snapshot are required %s\n", error.c_str());
        return exit_usage;
    }

    const std::filesystem::path folder = std::filesystem::u8path(argv[2]);
    const auto trusted = FileSnapshot::load(std::filesystem::u8path(snapshot_path));
    const FolderVerifier::Result result = FolderVerifier::check(folder, manifest, trusted ? &*trusted : nullptr, Progress());
    std::printf("status %d\nhashed_bytes %llu\nused_snapshot %s\n", int(result.status), static_cast<unsigned long long>(result.hashed_bytes),
                trusted && trusted->applies_to(folder, manifest.aggregate_sha256()) ? "yes" : "no");

    if (result.status != FolderVerifier::Result::Status::Verified) {
        return exit_failure;
    }

    if (!result.snapshot.save(std::filesystem::u8path(snapshot_path), &error)) {
        std::fprintf(stderr, "lom-disc-tool: %s\n", error.c_str());
        return exit_failure;
    }

    return 0;
}

/**
 * @brief Identify and install an image into a games root.
 * @param argc Argument count.
 * @param argv Arguments.
 * @return Process exit status.
 */
int install(int argc, char** argv) {
    ReleaseCatalog catalog;

    if (!load_catalog(argc, argv, catalog)) {
        return exit_failure;
    }

    const std::string games_root = option(argc, argv, "--games-root");
    const DiscImporter importer(catalog);
    const DiscImporter::Identification identification = importer.identify(std::filesystem::u8path(argv[2]), Progress());

    if (identification.status != DiscImporter::Identification::Status::Supported || games_root.empty()) {
        std::fprintf(stderr, "lom-disc-tool: the image is not a supported edition (status %d)\n", int(identification.status));
        return exit_failure;
    }

    FileManifest manifest;
    std::string error;

    if (!manifest.load(identification.release->manifest, &error)) {
        std::fprintf(stderr, "lom-disc-tool: %s\n", error.c_str());
        return exit_failure;
    }

    const DiscImporter::InstallResult result = importer.install(identification, manifest, std::filesystem::u8path(games_root), Progress());
    std::printf("status %d\ndata %s\n", int(result.status), result.installation.data_path.u8string().c_str());

    if (!result.error.empty()) {
        std::printf("error %s\n", result.error.c_str());
    }

    return result.status == DiscImporter::InstallResult::Status::Installed ? 0 : exit_failure;
}

/**
 * @brief List a memory card's files and save validation.
 * @param argv Arguments.
 * @return Process exit status.
 */
int card(char** argv) {
    MemoryCard memory_card;
    const MemoryCard::Status status = memory_card.load(std::filesystem::u8path(argv[2]));

    if (status != MemoryCard::Status::Ok) {
        std::printf("status %d\nerror %s\n", int(status), memory_card.error().c_str());
        return exit_failure;
    }

    for (const MemoryCard::File& file : memory_card.files()) {
        const int save = file.problem.empty() ? int(LomSave::validate(file.bytes.data(), file.bytes.size())) : -1;
        std::printf("%-20s blocks %d save-status %d %s\n", file.name.c_str(), file.blocks, save, file.problem.c_str());
    }

    return 0;
}

}

/**
 * @brief Dispatch a command.
 * @param argc Argument count.
 * @param argv Arguments.
 * @return Process exit status.
 */
int main(int argc, char** argv) {
    if (argc < 3) {
        std::fputs(usage, stderr);
        return exit_usage;
    }

    const std::string command = argv[1];

    if (command == "identify") {
        return identify(argc, argv);
    }

    if (command == "manifest") {
        return manifest(argc, argv);
    }

    if (command == "project-iso") {
        return project_iso(argc, argv);
    }

    if (command == "verify-folder") {
        return verify_folder(argc, argv);
    }

    if (command == "check-folder") {
        return check_folder(argc, argv);
    }

    if (command == "install") {
        return install(argc, argv);
    }

    if (command == "card") {
        return card(argv);
    }

    std::fputs(usage, stderr);
    return exit_usage;
}
