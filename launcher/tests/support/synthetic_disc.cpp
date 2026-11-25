#include "synthetic_disc.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <stdexcept>

namespace lom::launcher::test {

namespace {

constexpr std::size_t user_bytes = 2048;
constexpr std::size_t raw_bytes = 2352;
constexpr std::uint32_t pvd_lba = 16;
constexpr std::uint32_t root_lba = 18;
constexpr std::uint8_t submode_data = 0x08;
constexpr std::uint8_t submode_form2 = 0x20;
constexpr std::uint8_t flag_directory = 0x02;
constexpr std::uint32_t lead_in_frames = 150;

/**
 * @brief Write a both-endian 32-bit field.
 * @param out Field start (8 bytes).
 * @param value Value.
 */
void both32(std::uint8_t* out, std::uint32_t value) {
    for (int i = 0; i < 4; i++) {
        out[i] = std::uint8_t(value >> (8 * i));
        out[7 - i] = std::uint8_t(value >> (8 * i));
    }
}

/**
 * @brief Append one directory record.
 * @param out Directory bytes.
 * @param name Identifier bytes.
 * @param lba Extent.
 * @param size Extent size.
 * @param directory Whether the record names a directory.
 */
void append_record(std::vector<std::uint8_t>& out, const std::string& name, std::uint32_t lba, std::uint32_t size, bool directory) {
    std::size_t length = 33 + name.size();
    length += length % 2;
    std::vector<std::uint8_t> record(length, 0);
    record[0] = std::uint8_t(length);
    both32(&record[2], lba);
    both32(&record[10], size);
    record[25] = directory ? flag_directory : 0;
    record[32] = std::uint8_t(name.size());
    std::copy(name.begin(), name.end(), record.begin() + 33);
    out.insert(out.end(), record.begin(), record.end());
}

/**
 * @brief Convert a number to binary-coded decimal.
 * @param value Number below 100.
 * @return BCD byte.
 */
std::uint8_t bcd(std::uint32_t value) {
    return std::uint8_t(((value / 10) << 4) | (value % 10));
}

}

void SyntheticDisc::add(const std::string& path, std::vector<std::uint8_t> data) {
    files_.push_back(File{path, std::move(data), false});
}

void SyntheticDisc::add_form2(const std::string& path, std::vector<std::uint8_t> data) {
    files_.push_back(File{path, std::move(data), true});
}

std::vector<std::vector<std::uint8_t>> SyntheticDisc::sectors(std::vector<bool>& form2) const {
    std::map<std::string, std::vector<const File*>> directories;
    directories[""];

    for (const File& file : files_) {
        const std::size_t slash = file.path.find('/');
        directories[slash == std::string::npos ? "" : file.path.substr(0, slash)].push_back(&file);
    }

    std::map<std::string, std::uint32_t> directory_lba;
    std::uint32_t next = root_lba;

    for (const auto& [name, contents] : directories) {
        directory_lba[name] = next++;
    }

    std::map<const File*, std::uint32_t> file_lba;

    for (const File& file : files_) {
        file_lba[&file] = next;
        next += std::uint32_t((file.data.size() + user_bytes - 1) / user_bytes);
    }

    const std::uint32_t total = next + 1;
    std::vector<std::vector<std::uint8_t>> out(total, std::vector<std::uint8_t>(user_bytes, 0));
    form2.assign(total, false);

    std::vector<std::uint8_t>& pvd = out[pvd_lba];
    pvd[0] = 1;
    std::copy_n("CD001", 5, pvd.begin() + 1);
    pvd[6] = 1;
    both32(&pvd[80], total);
    pvd[128] = std::uint8_t(user_bytes & 0xFF);
    pvd[129] = std::uint8_t(user_bytes >> 8);
    std::vector<std::uint8_t> root_record;
    append_record(root_record, std::string(1, '\0'), root_lba, user_bytes, true);
    std::copy(root_record.begin(), root_record.end(), pvd.begin() + 156);

    out[pvd_lba + 1][0] = 255;
    std::copy_n("CD001", 5, out[pvd_lba + 1].begin() + 1);

    for (const auto& [name, contents] : directories) {
        std::vector<std::uint8_t> records;
        const std::uint32_t self = directory_lba[name];
        append_record(records, std::string(1, '\0'), self, user_bytes, true);
        append_record(records, std::string(1, '\1'), root_lba, user_bytes, true);

        if (name.empty()) {
            for (const auto& [child, unused] : directories) {
                if (!child.empty()) {
                    append_record(records, child, directory_lba[child], user_bytes, true);
                }
            }
        }

        for (const File* file : contents) {
            const std::size_t slash = file->path.find('/');
            const std::string leaf = slash == std::string::npos ? file->path : file->path.substr(slash + 1);
            append_record(records, leaf + ";1", file_lba[file], std::uint32_t(file->data.size()), false);
        }

        if (records.size() > user_bytes) {
            throw std::runtime_error("synthetic directory too large");
        }

        std::copy(records.begin(), records.end(), out[self].begin());
    }

    for (const File& file : files_) {
        std::uint32_t lba = file_lba[&file];

        for (std::size_t offset = 0; offset < file.data.size(); offset += user_bytes, lba++) {
            const std::size_t take = std::min(user_bytes, file.data.size() - offset);
            std::copy_n(file.data.begin() + std::ptrdiff_t(offset), take, out[lba].begin());
            form2[lba] = file.form2;
        }
    }

    return out;
}

std::vector<std::uint8_t> SyntheticDisc::raw() const {
    std::vector<bool> form2;
    const auto data = sectors(form2);
    std::vector<std::uint8_t> image(data.size() * raw_bytes, 0);

    for (std::size_t lba = 0; lba < data.size(); lba++) {
        std::uint8_t* sector = image.data() + lba * raw_bytes;
        sector[0] = 0;
        std::fill_n(sector + 1, 10, 0xFF);
        sector[11] = 0;
        const std::uint32_t frames = std::uint32_t(lba) + lead_in_frames;
        sector[12] = bcd(frames / (75 * 60));
        sector[13] = bcd((frames / 75) % 60);
        sector[14] = bcd(frames % 75);
        sector[15] = 2;
        const std::uint8_t submode = std::uint8_t(submode_data | (form2[lba] ? submode_form2 : 0));
        sector[18] = submode;
        sector[22] = submode;
        std::copy(data[lba].begin(), data[lba].end(), sector + 24);
    }

    return image;
}

std::vector<std::uint8_t> SyntheticDisc::cooked() const {
    std::vector<bool> form2;
    const auto data = sectors(form2);
    std::vector<std::uint8_t> image;

    for (const auto& sector : data) {
        image.insert(image.end(), sector.begin(), sector.end());
    }

    return image;
}

void SyntheticDisc::write(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
}

std::vector<std::uint8_t> SyntheticDisc::pattern(std::size_t size, std::uint32_t seed) {
    std::vector<std::uint8_t> bytes(size);
    std::uint32_t state = seed * 2654435761u + 1;

    for (std::uint8_t& byte : bytes) {
        state = state * 1664525u + 1013904223u;
        byte = std::uint8_t(state >> 24);
    }

    return bytes;
}

}
