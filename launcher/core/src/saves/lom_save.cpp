#include "lom/launcher/saves/lom_save.h"

namespace lom::launcher {

namespace {
constexpr std::uint8_t header_magic[2] = {'S', 'C'};
constexpr std::size_t checksum_bytes = 0x33E0;
constexpr std::size_t checksum_offset = checksum_bytes;
constexpr std::size_t magic_offset = checksum_offset + 4;
constexpr std::uint32_t checksum_multiplier = 2;
constexpr std::uint32_t checksum_bias = 0x0414E410u;
constexpr std::uint32_t save_magic = 0x00414E41u;

/**
 * @brief Read a little-endian 32-bit word.
 * @param bytes Word start.
 * @return Value.
 */
std::uint32_t le32(const std::uint8_t* bytes) {
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}

}

LomSave::Status LomSave::validate(const std::uint8_t* data, std::size_t size) {
    if (size != file_bytes) {
        return Status::WrongSize;
    }

    if (data[0] != header_magic[0] || data[1] != header_magic[1]) {
        return Status::BadHeader;
    }

    std::uint32_t sum = 0;

    for (std::size_t i = 0; i < checksum_bytes; i++) {
        sum += data[i];
    }

    if (le32(data + checksum_offset) != sum * checksum_multiplier + checksum_bias) {
        return Status::BadChecksum;
    }

    return le32(data + magic_offset) == save_magic ? Status::Valid : Status::BadMagic;
}

}
