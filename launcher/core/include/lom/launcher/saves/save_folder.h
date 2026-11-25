#ifndef LOM_LAUNCHER_SAVE_FOLDER_H
#define LOM_LAUNCHER_SAVE_FOLDER_H

#include "lom/launcher/saves/lom_save.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lom::launcher {

class ReleaseCatalog;

class SaveFolder {
  public:
    struct Save {
        int slot = 0;
        std::string name;
        std::uint64_t bytes = 0;
        std::string release_id;
        LomSave::Status status = LomSave::Status::Valid;
    };

    enum class Status { Ready, Empty, SlotDirectorySelected, Missing, NotADirectory, Unreadable, NotWritable, GameCannotRead, OtherRegion };

    struct Inspection {
        Status status = Status::Missing;
        std::filesystem::path suggested_root;
        std::vector<Save> saves;
        std::string error;
    };

    static constexpr int slot_count = 2;
    static constexpr int slot_blocks = 15;
    static constexpr std::uint64_t block_bytes = 8192;
    static constexpr std::size_t max_name_bytes = 20;

    /**
     * @brief Inspect a folder as a card root for one release.
     * @param root Folder to inspect.
     * @param catalog Releases, used to recognize save names.
     * @param release_id Release whose saves the folder should hold.
     * @return The inspection; Ready and Empty are usable.
     */
    static Inspection inspect(const std::filesystem::path& root, const ReleaseCatalog& catalog, const std::string& release_id);

    /**
     * @brief Create the slot directories of a new card root.
     * @param root Folder to initialize.
     * @param error Receives a description on failure; may be null.
     * @return True when both slot directories exist.
     */
    static bool initialize(const std::filesystem::path& root, std::string* error);

    /**
     * @brief Build the directory of a slot.
     * @param root Card root.
     * @param slot Slot number.
     * @return root/slotN.
     */
    static std::filesystem::path slot_directory(const std::filesystem::path& root, int slot);

    /**
     * @brief Check a card file name against the game's listing rules.
     * @param name File name.
     * @return True when the game can list it.
     */
    static bool valid_card_name(const std::string& name);
};

}

#endif
