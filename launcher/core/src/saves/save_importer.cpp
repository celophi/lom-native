#include "lom/launcher/saves/save_importer.h"
#include "lom/launcher/common/files_provider.h"

#include "lom/launcher/common/atomic_file.h"
#include "lom/launcher/saves/lom_save.h"
#include "lom/launcher/disc/release_catalog.h"
#include "lom/launcher/saves/save_folder.h"

#include <algorithm>
#include <istream>
#include <ostream>
#include <system_error>

namespace lom::launcher {

namespace {
struct Placed {
    std::filesystem::path destination;
    std::filesystem::path backup;
};

/**
 * @brief Write a whole file and read it back.
 * @param path Destination.
 * @param bytes Contents.
 * @return True when the read-back bytes are identical.
 */
bool write_verified(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    {
        const auto out_file = files_provider().open_write(path, std::ios::binary | std::ios::trunc);
        std::ostream& out = *out_file;
        out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));

        if (!out) {
            return false;
        }
    }

    std::vector<std::uint8_t> check;
    return AtomicFile::read_bytes(path, check) && check == bytes;
}

constexpr std::uint32_t operation_suffix_range = 1000000;

/**
 * @brief Create a unique operation identifier.
 * @param clock Time source.
 * @param random Random source.
 * @return Time-based identifier with a random suffix.
 */
std::string operation_id(const ClockProvider& clock, RandomProvider& random) {
    return std::to_string(clock.now_unix_seconds()) + "-" + std::to_string(random.next() % operation_suffix_range);
}

/**
 * @brief Undo moved files: remove new files and restore backed-up originals.
 * @param placed Files moved so far.
 */
void roll_back(const std::vector<Placed>& placed) {
    for (auto item = placed.rbegin(); item != placed.rend(); ++item) {
        std::error_code code;
        files_provider().remove(item->destination, code);

        if (!item->backup.empty()) {
            files_provider().copy_file(item->backup, item->destination, std::filesystem::copy_options::overwrite_existing, code);
        }
    }
}

}

SaveImporter::Plan SaveImporter::plan(const std::filesystem::path& root, int slot, const std::vector<MemoryCard::File>& files) {
    Plan plan;
    const std::filesystem::path directory = SaveFolder::slot_directory(root, slot);
    std::error_code code;

    for (const FileEntry& entry : files_provider().list(directory, false, code)) {
        const std::string name = entry.path.filename().u8string();

        if (!name.empty() && name[0] != '.' && entry.regular) {
            plan.blocks_used += int(entry.size / SaveFolder::block_bytes);
        }
    }

    for (const MemoryCard::File& file : files) {
        const std::filesystem::path existing = directory / std::filesystem::u8path(file.name);

        if (!files_provider().exists(existing, code)) {
            plan.new_names.push_back(file.name);
            plan.blocks_needed += file.blocks;
            continue;
        }

        std::vector<std::uint8_t> current;

        if (AtomicFile::read_bytes(existing, current) && current == file.bytes) {
            plan.identical_names.push_back(file.name);
        } else {
            plan.conflicting_names.push_back(file.name);
        }
    }

    return plan;
}

SaveImporter::Result SaveImporter::commit(const std::filesystem::path& root, int slot, const std::vector<MemoryCard::File>& files,
                                          const std::set<std::string>& replace, const ReleaseCatalog& catalog, const std::string& release_id,
                                          const ClockProvider& clock, RandomProvider& random) {
    Result result;

    for (const MemoryCard::File& file : files) {
        const ReleaseCatalog::Release* release = catalog.by_card_filename(file.name);

        if (!file.problem.empty() || !SaveFolder::valid_card_name(file.name) ||
            LomSave::validate(file.bytes.data(), file.bytes.size()) != LomSave::Status::Valid) {
            result.status = Result::Status::InvalidSave;
            result.error = file.name + " is not a valid Legend of Mana save";
            return result;
        }

        if (release == nullptr || release->id != release_id) {
            result.status = Result::Status::WrongEdition;
            result.error = file.name + " is a save from another edition of the game";
            return result;
        }
    }

    std::string error;

    if (!SaveFolder::initialize(root, &error)) {
        result.status = Result::Status::DestinationUnavailable;
        result.error = error;
        return result;
    }

    const Plan current = plan(root, slot, files);
    int blocks_after = current.blocks_used + current.blocks_needed;
    std::vector<const MemoryCard::File*> to_write;

    for (const MemoryCard::File& file : files) {
        const bool is_new = std::find(current.new_names.begin(), current.new_names.end(), file.name) != current.new_names.end();
        const bool conflicting = std::find(current.conflicting_names.begin(), current.conflicting_names.end(), file.name) != current.conflicting_names.end();

        if (is_new) {
            to_write.push_back(&file);
        } else if (conflicting && replace.count(file.name) != 0) {
            std::error_code code;
            const auto existing_bytes = files_provider().file_size(SaveFolder::slot_directory(root, slot) / std::filesystem::u8path(file.name), code);
            blocks_after += file.blocks - int(existing_bytes / SaveFolder::block_bytes);
            to_write.push_back(&file);
        } else if (conflicting) {
            result.kept_existing++;
        } else {
            result.skipped_identical++;
        }
    }

    if (to_write.empty()) {
        result.status = Result::Status::NothingToImport;
        return result;
    }

    if (blocks_after > SaveFolder::slot_blocks) {
        result.status = Result::Status::NoSpace;
        result.error = "Memory card " + std::to_string(slot + 1) + " does not have enough free blocks for these saves.";
        return result;
    }

    const std::string id = operation_id(clock, random);
    const std::filesystem::path staging = root / staging_directory / id;
    const std::filesystem::path backups = root / backups_directory / id / ("slot" + std::to_string(slot));
    std::error_code code;
    files_provider().create_directories(staging, code);

    if (code) {
        result.status = Result::Status::DestinationUnavailable;
        result.error = "cannot create " + staging.u8string() + ": " + code.message();
        return result;
    }

    for (const MemoryCard::File* file : to_write) {
        if (!write_verified(staging / std::filesystem::u8path(file->name), file->bytes)) {
            files_provider().remove_all(staging, code);
            result.status = Result::Status::DestinationUnavailable;
            result.error = "cannot write " + file->name + " (the disk may be full)";
            return result;
        }
    }

    std::vector<Placed> placed;
    const std::filesystem::path directory = SaveFolder::slot_directory(root, slot);

    for (const MemoryCard::File* file : to_write) {
        Placed item;
        item.destination = directory / std::filesystem::u8path(file->name);

        if (files_provider().exists(item.destination, code)) {
            files_provider().create_directories(backups, code);
            item.backup = backups / std::filesystem::u8path(file->name);

            if (code || !files_provider().copy_file(item.destination, item.backup, std::filesystem::copy_options::overwrite_existing, code)) {
                roll_back(placed);
                files_provider().remove_all(staging, code);
                result.status = Result::Status::Failed;
                result.error = "cannot back up " + file->name;
                return result;
            }

            result.backup_directory = backups;
        }

        files_provider().rename(staging / std::filesystem::u8path(file->name), item.destination, code);

        if (code) {
            roll_back(placed);
            files_provider().remove_all(staging, code);
            result.status = Result::Status::Failed;
            result.error = "cannot place " + file->name + ": " + code.message();
            return result;
        }

        placed.push_back(item);
        result.imported++;
    }

    files_provider().remove_all(staging, code);
    files_provider().remove(root / staging_directory, code);
    result.status = Result::Status::Imported;
    return result;
}

}
