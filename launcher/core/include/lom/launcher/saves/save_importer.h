#ifndef LOM_LAUNCHER_SAVE_IMPORTER_H
#define LOM_LAUNCHER_SAVE_IMPORTER_H

#include "lom/launcher/common/clock_provider.h"
#include "lom/launcher/saves/memory_card.h"
#include "lom/launcher/common/random_provider.h"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace lom::launcher {

class ReleaseCatalog;

class SaveImporter {
  public:
    struct Plan {
        std::vector<std::string> new_names;
        std::vector<std::string> identical_names;
        std::vector<std::string> conflicting_names;
        int blocks_used = 0;
        int blocks_needed = 0;
    };

    struct Result {
        enum class Status { Imported, NothingToImport, NoSpace, InvalidSave, WrongEdition, DestinationUnavailable, Failed };

        Status status = Status::Failed;
        int imported = 0;
        int skipped_identical = 0;
        int kept_existing = 0;
        std::filesystem::path backup_directory;
        std::string error;
    };

    static constexpr char staging_directory[] = ".import-staging";
    static constexpr char backups_directory[] = "backups";

    /**
     * @brief Describe what importing files into a slot would do.
     * @param root Native card root.
     * @param slot Destination slot.
     * @param files Files selected from the card.
     * @return The plan.
     */
    static Plan plan(const std::filesystem::path& root, int slot, const std::vector<MemoryCard::File>& files);

    /**
     * @brief Import files into a slot.
     * @param root Native card root for release_id.
     * @param slot Destination slot.
     * @param files Files selected from the card; each must be a valid save of release_id.
     * @param replace Names whose differing existing files the user chose to replace.
     * @param catalog Releases, to confirm each save's edition.
     * @param release_id Edition of the card root.
     * @param clock Time source for the backup folder name.
     * @param random Random source for the backup folder name.
     * @return The result; on failure the slot is restored to its previous contents.
     */
    static Result commit(const std::filesystem::path& root, int slot, const std::vector<MemoryCard::File>& files, const std::set<std::string>& replace,
                         const ReleaseCatalog& catalog, const std::string& release_id, const ClockProvider& clock = ClockProvider::system(),
                         RandomProvider& random = RandomProvider::system());
};

}

#endif
