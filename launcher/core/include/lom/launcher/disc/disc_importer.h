#ifndef LOM_LAUNCHER_DISC_IMPORTER_H
#define LOM_LAUNCHER_DISC_IMPORTER_H

#include "lom/launcher/common/clock_provider.h"
#include "lom/launcher/disc/file_snapshot.h"
#include "lom/launcher/disc/installation.h"
#include "lom/launcher/common/progress.h"
#include "lom/launcher/common/random_provider.h"
#include "lom/launcher/disc/release_catalog.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lom::launcher {

class FileManifest;

class DiscImporter {
  public:
    struct Identification {
        enum class Status {
            Supported,
            UnknownIdentity,
            NoTrustedFileList,
            NotFound,
            Unreadable,
            Malformed,
            MissingTrackFile,
            UnsupportedLayout,
            UnsupportedFormat,
            Cancelled
        };

        enum class Basis { RawIdentity, FileContents };

        Status status = Status::Unreadable;
        Basis basis = Basis::RawIdentity;
        std::filesystem::path selected;
        std::string layout;
        std::string sha1;
        std::uint64_t bytes = 0;
        const ReleaseCatalog::Release* release = nullptr;
        const ReleaseCatalog::Release* recognized = nullptr;
        std::vector<std::string> problems;
        std::string error;
    };

    struct InstallResult {
        enum class Status { Installed, NoManifest, DestinationUnavailable, InsufficientSpace, SourceChanged, ExtractionFailed, VerificationFailed, Cancelled };

        Status status = Status::ExtractionFailed;
        Installation installation;
        std::uint64_t required_bytes = 0;
        std::string error;
        std::vector<std::string> problems;
        std::optional<FileSnapshot> snapshot;
    };

    static constexpr char staging_directory[] = ".staging";
    static constexpr char data_directory[] = "data";
    static constexpr char receipt_file[] = "receipt.json";
    static constexpr int extractor_version = 1;

    /**
     * @brief Bind the importer to a catalog.
     * @param catalog Supported releases; must outlive the importer.
     * @param clock Time source for installation names and receipts; must outlive the importer.
     * @param random Random source for installation names; must outlive the importer.
     */
    explicit DiscImporter(const ReleaseCatalog& catalog, const ClockProvider& clock = ClockProvider::system(), RandomProvider& random = RandomProvider::system());

    /**
     * @brief Classify and hash an image, then look up its identity.
     * @param selected The file the user chose.
     * @param progress CheckingImage progress and cancellation.
     * @return The identification.
     */
    Identification identify(const std::filesystem::path& selected, const Progress& progress) const;

    /**
     * @brief Extract a Supported image into a new installation under games_root.
     * @param identification Result of identify() for the same image.
     * @param manifest Trusted manifest for the identified release.
     * @param games_root Root for managed game data.
     * @param progress Extracting and VerifyingFiles progress and cancellation.
     * @return The result; on Installed, installation describes the new data directory.
     */
    InstallResult install(const Identification& identification, const FileManifest& manifest, const std::filesystem::path& games_root,
                          const Progress& progress) const;

    /**
     * @brief Remove leftovers of interrupted imports.
     * @param games_root Root for managed game data.
     */
    static void remove_stale_staging(const std::filesystem::path& games_root);

    /**
     * @brief Estimate the free space an import needs.
     * @param manifest Trusted manifest.
     * @return Bytes, including a margin for receipts and file-system overhead.
     */
    static std::uint64_t required_space(const FileManifest& manifest);

  private:
    const ReleaseCatalog& catalog_;
    const ClockProvider& clock_;
    RandomProvider& random_;
};

}

#endif
