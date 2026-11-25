#include "lom/launcher/disc/iso9660_reader.h"

#include "lom/launcher/disc/disc_image.h"
#include "common/text_case.h"

#include <algorithm>
#include <set>

namespace lom::launcher {

namespace {

constexpr std::uint32_t volume_descriptor_lba = 16;
constexpr std::uint8_t primary_volume_type = 1;
constexpr std::size_t logical_block_size_offset = 128;
constexpr std::size_t root_record_offset = 156;
constexpr std::size_t record_extent_offset = 2;
constexpr std::size_t record_size_offset = 10;
constexpr std::size_t record_flags_offset = 25;
constexpr std::size_t record_name_length_offset = 32;
constexpr std::size_t record_name_offset = 33;
constexpr std::size_t minimum_record_bytes = 34;
constexpr std::uint8_t flag_directory = 0x02;
constexpr std::uint8_t flag_associated = 0x04;
constexpr std::uint8_t flag_multi_extent = 0x80;
constexpr std::uint32_t max_directory_bytes = 1u << 20;
constexpr std::uint32_t max_system_cnf_bytes = 4096;
constexpr char system_cnf_path[] = "SYSTEM.CNF";
constexpr char boot_key[] = "BOOT";
constexpr char boot_device_prefix[] = "cdrom:";

/**
 * @brief Read a little-endian 32-bit field.
 * @param bytes Field start.
 * @return Value.
 */
std::uint32_t le32(const std::uint8_t* bytes) {
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}

/**
 * @brief Read a little-endian 16-bit field.
 * @param bytes Field start.
 * @return Value.
 */
std::uint32_t le16(const std::uint8_t* bytes) {
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8);
}

/**
 * @brief Convert a stored file identifier into a safe path component.
 * @param raw Identifier bytes.
 * @param component Receives the name without the version suffix or a trailing dot.
 * @return False when the name is empty or contains separators, controls or "..".
 */
bool clean_component(const std::string& raw, std::string& component) {
    component = raw;
    const std::size_t version = component.find(';');

    if (version != std::string::npos) {
        component.erase(version);
    }

    if (!component.empty() && component.back() == '.') {
        component.pop_back();
    }

    if (component.empty() || component == "." || component == "..") {
        return false;
    }

    for (const char c : component) {
        if (static_cast<unsigned char>(c) < 0x20 || c == '/' || c == '\\' || c == ':') {
            return false;
        }
    }

    return true;
}

}

bool Iso9660Reader::read(DiscImage& image) {
    files_.clear();
    visited_directories_.clear();
    error_.clear();
    std::uint8_t sector[DiscImage::user_sector_bytes];

    if (!image.read_user_data(volume_descriptor_lba, sector, nullptr)) {
        error_ = image.error();
        return false;
    }

    if (sector[0] != primary_volume_type || le16(sector + logical_block_size_offset) != DiscImage::user_sector_bytes) {
        error_ = "the primary volume descriptor is missing or uses an unsupported block size";
        return false;
    }

    const std::uint8_t* root = sector + root_record_offset;

    if (!walk(image, le32(root + record_extent_offset), le32(root + record_size_offset), "", 0)) {
        return false;
    }

    std::set<std::string> seen;

    for (const File& file : files_) {
        if (!seen.insert(ascii_upper(file.path)).second) {
            error_ = "the disc lists " + file.path + " more than once";
            return false;
        }
    }

    return true;
}

bool Iso9660Reader::walk(DiscImage& image, std::uint32_t lba, std::uint32_t bytes, const std::string& prefix, int depth) {
    if (depth > max_depth || bytes == 0 || bytes > max_directory_bytes) {
        error_ = "directory " + prefix + " has an invalid size or depth";
        return false;
    }

    if (std::find(visited_directories_.begin(), visited_directories_.end(), lba) != visited_directories_.end()) {
        error_ = "directory " + prefix + " loops back to an earlier directory";
        return false;
    }

    visited_directories_.push_back(lba);
    const std::uint32_t sectors = (bytes + DiscImage::user_sector_bytes - 1) / DiscImage::user_sector_bytes;

    if (std::uint64_t(lba) + sectors > image.sector_count()) {
        error_ = "directory " + prefix + " extends past the end of the image";
        return false;
    }

    std::vector<std::uint8_t> data(std::size_t(sectors) * DiscImage::user_sector_bytes);

    for (std::uint32_t i = 0; i < sectors; i++) {
        if (!image.read_user_data(lba + i, data.data() + std::size_t(i) * DiscImage::user_sector_bytes, nullptr)) {
            error_ = image.error();
            return false;
        }
    }

    std::size_t offset = 0;

    while (offset < bytes) {
        const std::size_t length = data[offset];

        if (length == 0) {
            offset = (offset / DiscImage::user_sector_bytes + 1) * DiscImage::user_sector_bytes;
            continue;
        }

        const std::size_t sector_end = (offset / DiscImage::user_sector_bytes + 1) * DiscImage::user_sector_bytes;

        if (length < minimum_record_bytes || offset + length > sector_end) {
            error_ = "directory " + prefix + " has a malformed record";
            return false;
        }

        const std::uint8_t* record = data.data() + offset;
        const std::size_t name_length = record[record_name_length_offset];

        if (record_name_offset + name_length > length) {
            error_ = "directory " + prefix + " has a malformed name";
            return false;
        }

        const std::uint8_t flags = record[record_flags_offset];
        const std::uint32_t extent = le32(record + record_extent_offset);
        const std::uint32_t size = le32(record + record_size_offset);
        offset += length;

        if (name_length == 1 && (record[record_name_offset] == 0 || record[record_name_offset] == 1)) {
            continue;
        }

        if ((flags & (flag_associated | flag_multi_extent)) != 0) {
            error_ = "directory " + prefix + " uses associated or multi-extent files, which are not supported";
            return false;
        }

        std::string component;

        if (!clean_component(std::string(reinterpret_cast<const char*>(record + record_name_offset), name_length), component)) {
            error_ = "directory " + prefix + " contains an unsafe file name";
            return false;
        }

        const std::string path = prefix + component;

        if ((flags & flag_directory) != 0) {
            if (!walk(image, extent, size, path + "/", depth + 1)) {
                return false;
            }

            continue;
        }

        const std::uint64_t sectors_used = (std::uint64_t(size) + DiscImage::user_sector_bytes - 1) / DiscImage::user_sector_bytes;

        if (std::uint64_t(extent) + sectors_used > image.sector_count()) {
            error_ = path + " extends past the end of the image";
            return false;
        }

        if (files_.size() >= max_files) {
            error_ = "the disc lists too many files";
            return false;
        }

        files_.push_back(File{path, extent, size});
    }

    return true;
}

std::string Iso9660Reader::boot_executable(DiscImage& image) const {
    for (const File& file : files_) {
        if (file.path != system_cnf_path || file.bytes > max_system_cnf_bytes) {
            continue;
        }

        std::vector<std::uint8_t> bytes;

        if (!read_file(image, file, bytes)) {
            return {};
        }

        const std::string text(bytes.begin(), bytes.end());
        const std::size_t key = text.find(boot_key);

        if (key == std::string::npos) {
            return {};
        }

        std::size_t start = text.find(boot_device_prefix, key);

        if (start == std::string::npos) {
            return {};
        }

        start += sizeof(boot_device_prefix) - 1;

        while (start < text.size() && (text[start] == '\\' || text[start] == '/')) {
            start++;
        }

        std::size_t end = start;

        while (end < text.size() && text[end] != ';' && text[end] != '\r' && text[end] != '\n' && text[end] != ' ') {
            end++;
        }

        return ascii_upper(text.substr(start, end - start));
    }

    return {};
}

bool Iso9660Reader::read_file(DiscImage& image, const File& file, std::vector<std::uint8_t>& out) {
    out.resize(file.bytes);
    std::uint8_t sector[DiscImage::user_sector_bytes];
    std::uint32_t done = 0;
    std::uint32_t lba = file.lba;

    while (done < file.bytes) {
        DiscImage::SectorKind kind;

        if (!image.read_user_data(lba, sector, &kind) || kind == DiscImage::SectorKind::Mode2Form2) {
            return false;
        }

        const std::uint32_t take = std::min<std::uint32_t>(DiscImage::user_sector_bytes, file.bytes - done);
        std::copy(sector, sector + take, out.begin() + done);
        done += take;
        lba++;
    }

    return true;
}

const std::vector<Iso9660Reader::File>& Iso9660Reader::files() const {
    return files_;
}

const std::string& Iso9660Reader::error() const {
    return error_;
}

}
