#include "lom/launcher/common/zip_writer.h"

#include <array>

namespace lom::launcher {

namespace {

constexpr std::uint32_t local_header_signature = 0x04034b50u;
constexpr std::uint32_t central_header_signature = 0x02014b50u;
constexpr std::uint32_t end_signature = 0x06054b50u;
constexpr std::uint16_t zip_version = 20;
constexpr std::uint16_t utf8_flag = 0x0800;
constexpr std::uint16_t method_stored = 0;
constexpr std::uint16_t method_deflate = 8;
constexpr std::uint32_t crc_polynomial = 0xEDB88320u;
constexpr int dos_epoch_year = 1980;

/**
 * @brief Append a little-endian 16-bit value.
 * @param out Bytes so far.
 * @param value Value.
 */
void put16(std::string& out, std::uint16_t value) {
    out.push_back(char(value & 0xff));
    out.push_back(char(value >> 8));
}

/**
 * @brief Append a little-endian 32-bit value.
 * @param out Bytes so far.
 * @param value Value.
 */
void put32(std::string& out, std::uint32_t value) {
    put16(out, std::uint16_t(value & 0xffff));
    put16(out, std::uint16_t(value >> 16));
}

/**
 * @brief Build the CRC-32 lookup table.
 * @return One entry per byte value.
 */
std::array<std::uint32_t, 256> make_crc_table() {
    std::array<std::uint32_t, 256> table{};

    for (std::uint32_t i = 0; i < table.size(); ++i) {
        std::uint32_t value = i;

        for (int bit = 0; bit < 8; ++bit) {
            value = value & 1u ? crc_polynomial ^ (value >> 1) : value >> 1;
        }

        table[i] = value;
    }

    return table;
}

}

ZipWriter::ZipWriter(Compressor compressor) : compressor_(std::move(compressor)) {
}

std::uint32_t ZipWriter::crc32(const std::string& data) {
    static const std::array<std::uint32_t, 256> table = make_crc_table();
    std::uint32_t crc = 0xffffffffu;

    for (unsigned char byte : data) {
        crc = table[(crc ^ byte) & 0xffu] ^ (crc >> 8);
    }

    return crc ^ 0xffffffffu;
}

void ZipWriter::add(const std::string& name, const std::string& data, std::time_t modified) {
    Entry entry;
    entry.name = name;
    entry.crc = crc32(data);
    entry.size = std::uint32_t(data.size());
    entry.offset = std::uint32_t(body_.size());

    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &modified);
#else
    localtime_r(&modified, &local);
#endif

    if (local.tm_year + 1900 < dos_epoch_year) {
        local = std::tm{};
        local.tm_year = dos_epoch_year - 1900;
        local.tm_mday = 1;
    }

    entry.dos_time = std::uint16_t((local.tm_hour << 11) | (local.tm_min << 5) | (local.tm_sec / 2));
    entry.dos_date = std::uint16_t(((local.tm_year + 1900 - dos_epoch_year) << 9) | ((local.tm_mon + 1) << 5) | local.tm_mday);

    std::optional<std::string> packed;

    if (compressor_ && !data.empty()) {
        packed = compressor_(data);
    }

    const bool deflated = packed && packed->size() < data.size();
    const std::string& stored = deflated ? *packed : data;
    entry.method = deflated ? method_deflate : method_stored;
    entry.compressed_size = std::uint32_t(stored.size());

    put32(body_, local_header_signature);
    put16(body_, zip_version);
    put16(body_, utf8_flag);
    put16(body_, entry.method);
    put16(body_, entry.dos_time);
    put16(body_, entry.dos_date);
    put32(body_, entry.crc);
    put32(body_, entry.compressed_size);
    put32(body_, entry.size);
    put16(body_, std::uint16_t(name.size()));
    put16(body_, 0);
    body_ += name;
    body_ += stored;
    entries_.push_back(std::move(entry));
}

std::string ZipWriter::finish() const {
    std::string archive = body_;
    const std::uint32_t directory_offset = std::uint32_t(archive.size());

    for (const Entry& entry : entries_) {
        put32(archive, central_header_signature);
        put16(archive, zip_version);
        put16(archive, zip_version);
        put16(archive, utf8_flag);
        put16(archive, entry.method);
        put16(archive, entry.dos_time);
        put16(archive, entry.dos_date);
        put32(archive, entry.crc);
        put32(archive, entry.compressed_size);
        put32(archive, entry.size);
        put16(archive, std::uint16_t(entry.name.size()));
        put16(archive, 0);
        put16(archive, 0);
        put16(archive, 0);
        put16(archive, 0);
        put32(archive, 0);
        put32(archive, entry.offset);
        archive += entry.name;
    }

    const std::uint32_t directory_size = std::uint32_t(archive.size()) - directory_offset;
    put32(archive, end_signature);
    put16(archive, 0);
    put16(archive, 0);
    put16(archive, std::uint16_t(entries_.size()));
    put16(archive, std::uint16_t(entries_.size()));
    put32(archive, directory_size);
    put32(archive, directory_offset);
    put16(archive, 0);
    return archive;
}

}
