#include "lom/launcher/saves/save_folder.h"

#include "lom/launcher/common/atomic_file.h"
#include "lom/launcher/disc/release_catalog.h"

#include <chrono>
#include <fstream>
#include <random>
#include <system_error>
#include <vector>

namespace lom::launcher {

namespace {

constexpr char slot_prefix[] = "slot";
constexpr char write_probe_prefix[] = ".lom-write-check-";

/**
 * @brief Check that a directory accepts new files, leaving nothing behind.
 * @param directory Directory to probe.
 * @return True when a probe file could be created and removed.
 */
bool directory_writable(const std::filesystem::path& directory) {
    std::random_device device;
    const std::filesystem::path probe = directory / (write_probe_prefix + std::to_string(device()));

    {
        std::ofstream out(probe, std::ios::binary);

        if (!out) {
            return false;
        }
    }

    std::error_code code;
    std::filesystem::remove(probe, code);
    return true;
}

}

SaveFolder::Inspection SaveFolder::inspect(const std::filesystem::path& root, const ReleaseCatalog& catalog, const std::string& release_id) {
    Inspection result;
    std::error_code code;

    if (!std::filesystem::exists(root, code)) {
        result.status = Status::Missing;
        result.error = "the folder is not available";
        return result;
    }

    if (!std::filesystem::is_directory(root, code)) {
        result.status = Status::NotADirectory;
        result.error = "the selected path is not a folder";
        return result;
    }

    const std::string name = root.filename().u8string();

    if (name.size() == sizeof(slot_prefix) && name.compare(0, sizeof(slot_prefix) - 1, slot_prefix) == 0 && name.back() >= '0' &&
        name.back() < char('0' + slot_count)) {
        result.status = Status::SlotDirectorySelected;
        result.suggested_root = root.parent_path();
        result.error = "this is a slot folder; choose the folder that contains it";
        return result;
    }

    for (int slot = 0; slot < slot_count; slot++) {
        const std::filesystem::path directory = slot_directory(root, slot);

        if (!std::filesystem::exists(directory, code)) {
            continue;
        }

        if (!std::filesystem::is_directory(directory, code)) {
            result.status = Status::GameCannotRead;
            result.error = directory.filename().u8string() + " is not a folder";
            return result;
        }

        std::uint64_t used_blocks = 0;

        for (const auto& entry : std::filesystem::directory_iterator(directory, code)) {
            const std::string file_name = entry.path().filename().u8string();

            if (file_name.empty() || file_name[0] == '.') {
                continue;
            }

            const std::uint64_t bytes = entry.is_regular_file(code) && !entry.is_symlink(code) ? entry.file_size(code) : 0;

            if (code || !valid_card_name(file_name) || bytes == 0 || bytes % block_bytes != 0) {
                result.status = Status::GameCannotRead;
                result.error = "the " + directory.filename().u8string() + " folder contains " + file_name + ", which is not a memory card save";
                return result;
            }

            used_blocks += bytes / block_bytes;
            Save save;
            save.slot = slot;
            save.name = file_name;
            save.bytes = bytes;

            if (const ReleaseCatalog::Release* release = catalog.by_card_filename(file_name)) {
                save.release_id = release->id;
                std::vector<std::uint8_t> contents;
                save.status = AtomicFile::read_bytes(entry.path(), contents) ? LomSave::validate(contents.data(), contents.size()) : LomSave::Status::WrongSize;
            }

            result.saves.push_back(save);
        }

        if (code) {
            result.status = Status::Unreadable;
            result.error = "cannot read " + directory.u8string() + ": " + code.message();
            return result;
        }

        if (used_blocks > std::uint64_t(slot_blocks)) {
            result.status = Status::GameCannotRead;
            result.error = "the " + directory.filename().u8string() + " folder holds more than " + std::to_string(slot_blocks) + " blocks";
            return result;
        }
    }

    for (const Save& save : result.saves) {
        if (!save.release_id.empty() && save.release_id != release_id) {
            result.status = Status::OtherRegion;
            result.error = save.name + " is a save from another edition of the game";
            return result;
        }
    }

    bool slots_writable = true;

    for (int slot = 0; slot < slot_count; slot++) {
        const std::filesystem::path directory = slot_directory(root, slot);

        if (std::filesystem::exists(directory, code) && !directory_writable(directory)) {
            slots_writable = false;
        }
    }

    if (!directory_writable(root) || !slots_writable) {
        result.status = Status::NotWritable;
        result.error = "the folder is read-only, so the game could not save there";
        return result;
    }

    result.status = result.saves.empty() ? Status::Empty : Status::Ready;
    return result;
}

bool SaveFolder::initialize(const std::filesystem::path& root, std::string* error) {
    for (int slot = 0; slot < slot_count; slot++) {
        std::error_code code;
        std::filesystem::create_directories(slot_directory(root, slot), code);

        if (code) {
            if (error != nullptr) {
                *error = "cannot create " + slot_directory(root, slot).u8string() + ": " + code.message();
            }

            return false;
        }
    }

    return true;
}

std::filesystem::path SaveFolder::slot_directory(const std::filesystem::path& root, int slot) {
    return root / (slot_prefix + std::to_string(slot));
}

bool SaveFolder::valid_card_name(const std::string& name) {
    if (name.empty() || name.size() > max_name_bytes || name[0] == '.') {
        return false;
    }

    for (const char c : name) {
        if (static_cast<unsigned char>(c) < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?') {
            return false;
        }
    }

    return true;
}

}
