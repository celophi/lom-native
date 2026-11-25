#ifndef LOM_LAUNCHER_FOLDER_VERIFIER_H
#define LOM_LAUNCHER_FOLDER_VERIFIER_H

#include "lom/launcher/disc/file_snapshot.h"
#include "lom/launcher/common/progress.h"
#include "lom/launcher/disc/release_catalog.h"

#include <filesystem>
#include <string>
#include <vector>

namespace lom::launcher {

class FileManifest;

class FolderVerifier {
  public:
    struct Result {
        enum class Status {
            Verified,
            NotFound,
            NotAGameFolder,
            UnknownContents,
            RegionUnavailable,
            LayoutUnsupported,
            UnsafeLink,
            Unreadable,
            ChangedDuringCheck,
            Cancelled
        };

        Status status = Status::Unreadable;
        const ReleaseCatalog::Release* release = nullptr;
        std::vector<std::string> missing;
        std::vector<std::string> unexpected;
        std::vector<std::string> mismatched;
        std::vector<std::string> unreadable;
        std::vector<std::string> renamed;
        std::string error;
        FileSnapshot snapshot;
        std::uint64_t hashed_bytes = 0;
    };

    static constexpr std::size_t max_files = 20000;

    /**
     * @brief Bind the verifier to a catalog.
     * @param catalog Supported releases; must outlive the verifier.
     */
    explicit FolderVerifier(const ReleaseCatalog& catalog);

    /**
     * @brief Identify a user-selected folder's release and verify it completely.
     * @param root Folder containing the extracted disc tree.
     * @param progress CheckingFolder progress in bytes and cancellation.
     * @return The verification result.
     */
    Result verify(const std::filesystem::path& root, const Progress& progress) const;

    /**
     * @brief Compare a folder with one manifest, re-hashing only what changed since a trusted snapshot.
     * @param root Folder containing the extracted disc tree.
     * @param manifest Trusted manifest.
     * @param trusted Snapshot from the last full verification of this folder, or null to hash every file.
     *        The complete file list and every length are always checked.
     * @param progress CheckingFolder progress in bytes and cancellation.
     * @return The verification result; release is left null.
     */
    static Result check(const std::filesystem::path& root, const FileManifest& manifest, const FileSnapshot* trusted, const Progress& progress);

  private:
    const ReleaseCatalog& catalog_;
};

}

#endif
