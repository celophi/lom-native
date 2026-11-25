#include "lom/launcher/disc/disc_image.h"

#include "lom/launcher/disc/cue_sheet.h"
#include "lom/launcher/common/sha1.h"
#include "common/text_case.h"

#include <cstring>
#include <system_error>
#include <vector>

namespace lom::launcher {

namespace {
constexpr std::uint32_t volume_descriptor_lba = 16;
constexpr std::uint8_t sync_pattern[12] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
constexpr std::size_t mode_offset = 15;
constexpr std::size_t mode1_data_offset = 16;
constexpr std::size_t mode2_subheader_submode_offset = 18;
constexpr std::size_t mode2_data_offset = 24;
constexpr std::uint8_t submode_form2_bit = 0x20;
constexpr char volume_identifier[] = "CD001";
constexpr std::size_t volume_identifier_offset = 1;
constexpr std::size_t hash_chunk_bytes = 1u << 20;
constexpr char cue_extension[] = ".CUE";
constexpr char supported_raw_track_modes[2][11] = {"MODE2/2352", "MODE1/2352"};

/**
 * @brief Check whether a raw sector begins with the CD sync pattern.
 * @param sector Raw sector bytes.
 * @return True for a raw data sector header.
 */
bool has_sync(const std::uint8_t* sector) {
    return std::memcmp(sector, sync_pattern, sizeof(sync_pattern)) == 0;
}

/**
 * @brief Check user data for the ISO9660 volume identifier.
 * @param data User data of the volume descriptor sector.
 * @return True when "CD001" is present.
 */
bool has_volume_identifier(const std::uint8_t* data) {
    return std::memcmp(data + volume_identifier_offset, volume_identifier, sizeof(volume_identifier) - 1) == 0;
}

}

DiscImage::Status DiscImage::open(const std::filesystem::path& selected) {
    representation_ = Representation::Unknown;
    error_.clear();
    std::error_code code;

    if (!std::filesystem::exists(selected, code)) {
        error_ = "the selected file does not exist";
        return Status::NotFound;
    }

    if (ascii_upper(selected.extension().u8string()) != cue_extension) {
        return open_data(selected, false);
    }

    CueSheet sheet;
    const CueSheet::Status sheet_status = sheet.load(selected);

    if (sheet_status == CueSheet::Status::Unreadable) {
        error_ = sheet.error();
        return Status::Unreadable;
    }

    if (sheet_status != CueSheet::Status::Ok) {
        error_ = sheet.error();
        return Status::Malformed;
    }

    if (sheet.files().size() != 1 || sheet.tracks().size() != 1) {
        error_ = "the CUE lists " + std::to_string(sheet.tracks().size()) + " tracks in " + std::to_string(sheet.files().size()) +
                 " files; only a single data track is supported";
        return Status::UnsupportedLayout;
    }

    const CueSheet::Track& track = sheet.tracks().front();
    bool supported_mode = false;

    for (const char* mode : supported_raw_track_modes) {
        supported_mode = supported_mode || track.mode == mode;
    }

    if (track.number != 1 || !supported_mode || track.index01_frames != 0) {
        error_ = "track " + std::to_string(track.number) + " " + track.mode + " is not a supported single raw data track";
        return Status::UnsupportedLayout;
    }

    if (!std::filesystem::exists(sheet.files().front(), code)) {
        error_ = "the CUE refers to " + sheet.files().front().filename().u8string() + ", which is missing";
        return Status::MissingTrackFile;
    }

    return open_data(sheet.files().front(), true);
}

DiscImage::Status DiscImage::open_data(const std::filesystem::path& path, bool require_raw) {
    std::error_code code;
    data_file_ = path;
    data_bytes_ = std::filesystem::file_size(path, code);

    if (code) {
        error_ = "cannot read " + path.filename().u8string();
        return Status::Unreadable;
    }

    modified_ = std::filesystem::last_write_time(path, code);
    stream_.close();
    stream_.clear();
    stream_.open(path, std::ios::binary);

    if (!stream_) {
        error_ = "cannot open " + path.filename().u8string();
        return Status::Unreadable;
    }

    std::vector<std::uint8_t> sector(raw_sector_bytes);

    if (data_bytes_ % raw_sector_bytes == 0 && data_bytes_ >= std::uint64_t(volume_descriptor_lba + 1) * raw_sector_bytes) {
        stream_.seekg(std::streamoff(volume_descriptor_lba) * raw_sector_bytes);
        stream_.read(reinterpret_cast<char*>(sector.data()), raw_sector_bytes);

        if (stream_ && has_sync(sector.data())) {
            const std::uint8_t mode = sector[mode_offset];
            const std::size_t offset = mode == 1 ? mode1_data_offset : mode2_data_offset;

            if ((mode == 1 || mode == 2) && has_volume_identifier(sector.data() + offset)) {
                representation_ = Representation::RawSectors;
                return Status::Ok;
            }
        }
    }

    stream_.clear();

    if (require_raw) {
        error_ = "the track file does not contain raw 2352-byte data sectors";
        return Status::Malformed;
    }

    if (data_bytes_ % user_sector_bytes == 0 && data_bytes_ >= std::uint64_t(volume_descriptor_lba + 1) * user_sector_bytes) {
        stream_.seekg(std::streamoff(volume_descriptor_lba) * user_sector_bytes);
        stream_.read(reinterpret_cast<char*>(sector.data()), user_sector_bytes);

        if (stream_ && has_volume_identifier(sector.data())) {
            representation_ = Representation::CookedSectors;
            return Status::Ok;
        }
    }

    stream_.clear();
    error_ = "not a recognized CD image (no ISO9660 volume in raw or 2048-byte sectors)";
    return Status::UnsupportedFormat;
}

DiscImage::Representation DiscImage::representation() const {
    return representation_;
}

std::uint64_t DiscImage::data_bytes() const {
    return data_bytes_;
}

std::uint32_t DiscImage::sector_count() const {
    const std::uint32_t sector_bytes = representation_ == Representation::RawSectors ? raw_sector_bytes : user_sector_bytes;
    return std::uint32_t(data_bytes_ / sector_bytes);
}

const char* DiscImage::layout() const {
    return representation_ == Representation::RawSectors ? raw_single_track_layout : cooked_layout;
}

bool DiscImage::hash_identity(const Progress& progress, std::string& sha1) {
    std::ifstream in(data_file_, std::ios::binary);

    if (!in) {
        error_ = "cannot open " + data_file_.filename().u8string();
        return false;
    }

    Sha1 hash;
    std::vector<char> buffer(hash_chunk_bytes);
    std::uint64_t done = 0;

    while (done < data_bytes_) {
        if (progress.cancelled()) {
            error_ = "cancelled";
            return false;
        }

        in.read(buffer.data(), std::streamsize(buffer.size()));
        const std::streamsize got = in.gcount();

        if (got <= 0) {
            break;
        }

        hash.update(buffer.data(), std::size_t(got));
        done += std::uint64_t(got);
        progress.report(Progress::Stage::CheckingImage, done, data_bytes_);
    }

    if (done != data_bytes_ || !unchanged_since_open()) {
        error_ = "the image could not be read completely or changed while it was checked";
        return false;
    }

    sha1 = hash.hex_digest();
    return true;
}

bool DiscImage::read_user_data(std::uint32_t lba, std::uint8_t* out, SectorKind* kind) {
    if (lba >= sector_count()) {
        error_ = "sector " + std::to_string(lba) + " is outside the image";
        return false;
    }

    if (representation_ == Representation::CookedSectors) {
        stream_.seekg(std::streamoff(lba) * user_sector_bytes);
        stream_.read(reinterpret_cast<char*>(out), user_sector_bytes);

        if (kind != nullptr) {
            *kind = SectorKind::Cooked;
        }

        if (!stream_) {
            stream_.clear();
            error_ = "cannot read sector " + std::to_string(lba);
            return false;
        }

        return true;
    }

    std::uint8_t sector[raw_sector_bytes];
    stream_.seekg(std::streamoff(lba) * raw_sector_bytes);
    stream_.read(reinterpret_cast<char*>(sector), raw_sector_bytes);

    if (!stream_ || !has_sync(sector)) {
        stream_.clear();
        error_ = "sector " + std::to_string(lba) + " is not a readable data sector";
        return false;
    }

    const std::uint8_t mode = sector[mode_offset];
    SectorKind sector_kind;
    std::size_t offset;

    if (mode == 1) {
        sector_kind = SectorKind::Mode1;
        offset = mode1_data_offset;
    } else if (mode == 2) {
        sector_kind = (sector[mode2_subheader_submode_offset] & submode_form2_bit) != 0 ? SectorKind::Mode2Form2 : SectorKind::Mode2Form1;
        offset = mode2_data_offset;
    } else {
        error_ = "sector " + std::to_string(lba) + " has unsupported mode " + std::to_string(mode);
        return false;
    }

    std::memcpy(out, sector + offset, user_sector_bytes);

    if (kind != nullptr) {
        *kind = sector_kind;
    }

    return true;
}

bool DiscImage::unchanged_since_open() const {
    std::error_code code;
    const std::uintmax_t size = std::filesystem::file_size(data_file_, code);

    if (code || size != data_bytes_) {
        return false;
    }

    const auto modified = std::filesystem::last_write_time(data_file_, code);
    return !code && modified == modified_;
}

const std::string& DiscImage::error() const {
    return error_;
}

}
