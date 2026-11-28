#include "lom/launcher/saves/memory_card.h"
#include "lom/launcher/common/files_provider.h"

#include <algorithm>
#include <cstring>
#include <istream>
#include <ostream>
#include <system_error>

namespace lom::launcher {

namespace {

constexpr std::uint8_t card_magic[2] = {'M', 'C'};
constexpr std::size_t checksum_offset = MemoryCard::frame_bytes - 1;
constexpr std::size_t size_offset = 4;
constexpr std::size_t next_offset = 8;
constexpr std::size_t name_offset = 0x0A;
constexpr std::size_t name_max_bytes = 20;
constexpr std::uint8_t state_first = 0x51;
constexpr std::uint8_t state_middle = 0x52;
constexpr std::uint8_t state_last = 0x53;
constexpr std::uint16_t next_none = 0xFFFF;
constexpr std::uintmax_t max_card_file_bytes = 512 * 1024;

struct Wrapper {
    const char* signature;
    std::size_t offset;
    const char* description;
};

constexpr Wrapper wrappers[] = {
    {"123-456-STD", 0, "a DexDrive (.gme) card"},
    {"VgsM", 0, "a VGS (.mem/.vgs) card"},
    {"\0VSP", 0, "a PS3 single-save export (.psv)"},
    {"Sony PS", 0, "a Smart Link card (.psm)"},
};

/**
 * @brief Compute a frame's XOR checksum.
 * @param frame Frame bytes.
 * @return XOR of the first 127 bytes.
 */
std::uint8_t frame_checksum(const std::uint8_t* frame) {
    std::uint8_t value = 0;

    for (std::size_t i = 0; i < checksum_offset; i++) {
        value ^= frame[i];
    }

    return value;
}

/**
 * @brief Read a little-endian 32-bit field.
 * @param bytes Field start.
 * @return Value.
 */
std::uint32_t le32(const std::uint8_t* bytes) {
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}

/**
 * @brief Read a printable file name from a directory frame.
 * @param frame Frame bytes.
 * @param name Receives the name.
 * @return False when the name is empty or contains unsafe characters.
 */
bool read_name(const std::uint8_t* frame, std::string& name) {
    name.clear();

    for (std::size_t i = 0; i < name_max_bytes; i++) {
        const std::uint8_t c = frame[name_offset + i];

        if (c == 0) {
            break;
        }

        if (c < 0x20 || c >= 0x7F || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?') {
            return false;
        }

        name.push_back(char(c));
    }

    return !name.empty() && name[0] != '.';
}

}

MemoryCard::Status MemoryCard::load(const std::filesystem::path& path) {
    std::error_code code;
    const std::uintmax_t size = files_provider().file_size(path, code);

    if (code) {
        error_ = "cannot read the memory card file";
        return Status::Unreadable;
    }

    if (size > max_card_file_bytes) {
        error_ = "the file is too large to be a memory card";
        return Status::WrongSize;
    }

    const auto in_file = files_provider().open_read(path, std::ios::binary);

    std::istream& in = *in_file;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(size));

    if (!in) {
        error_ = "cannot read the memory card file";
        return Status::Unreadable;
    }

    return parse(bytes);
}

MemoryCard::Status MemoryCard::parse(const std::vector<std::uint8_t>& bytes) {
    files_.clear();
    error_.clear();

    for (const Wrapper& wrapper : wrappers) {
        const std::size_t length = wrapper.signature[0] == '\0' ? 4 : std::strlen(wrapper.signature);

        if (bytes.size() >= wrapper.offset + length && std::memcmp(bytes.data() + wrapper.offset, wrapper.signature, length) == 0) {
            error_ = std::string("this is ") + wrapper.description + ", which is not supported yet";
            return Status::UnsupportedWrapper;
        }
    }

    if (bytes.size() != card_bytes) {
        error_ = "a raw memory card must be exactly 131072 bytes";
        return Status::WrongSize;
    }

    if (bytes[0] != card_magic[0] || bytes[1] != card_magic[1] || frame_checksum(bytes.data()) != bytes[checksum_offset]) {
        error_ = "the card header is missing or damaged";
        return Status::BadHeader;
    }

    std::vector<std::vector<int>> chains;
    int block_users[data_blocks] = {};

    for (int first = 0; first < data_blocks; first++) {
        const std::uint8_t* frame = bytes.data() + std::size_t(first + 1) * frame_bytes;

        if (frame[0] != state_first) {
            continue;
        }

        File file;
        file.first_block = first;

        if (!read_name(frame, file.name)) {
            file.name = "(block " + std::to_string(first + 1) + ")";
            file.problem = "invalid file name";
        }

        const std::uint32_t declared_size = le32(frame + size_offset);
        std::vector<int> chain;
        int block = first;

        while (true) {
            const std::uint8_t* entry = bytes.data() + std::size_t(block + 1) * frame_bytes;

            if (frame_checksum(entry) != entry[checksum_offset]) {
                file.problem = "damaged directory entry";
                break;
            }

            if (std::find(chain.begin(), chain.end(), block) != chain.end()) {
                file.problem = "block chain loops";
                break;
            }

            const std::uint8_t state = entry[0];

            if ((chain.empty() && state != state_first) || (!chain.empty() && state != state_middle && state != state_last)) {
                file.problem = "broken block chain";
                break;
            }

            chain.push_back(block);
            const std::uint16_t next = std::uint16_t(entry[next_offset] | (entry[next_offset + 1] << 8));

            if (next == next_none) {
                if (chain.size() > 1 && state != state_last) {
                    file.problem = "block chain ends early";
                }

                break;
            }

            if (next >= data_blocks || int(chain.size()) >= data_blocks) {
                file.problem = "block chain points outside the card";
                break;
            }

            block = next;
        }

        for (const int used : chain) {
            block_users[used]++;
        }

        file.blocks = int(chain.size());

        if (file.problem.empty() && declared_size != std::uint32_t(file.blocks) * block_bytes) {
            file.problem = "file size does not match its blocks";
        }

        chains.push_back(chain);
        files_.push_back(std::move(file));
    }

    for (std::size_t i = 0; i < files_.size(); i++) {
        File& file = files_[i];

        for (const int used : chains[i]) {
            if (block_users[used] > 1 && file.problem.empty()) {
                file.problem = "blocks are shared with another file";
            }
        }

        if (file.problem.empty()) {
            for (const int used : chains[i]) {
                const std::uint8_t* data = bytes.data() + std::size_t(used + 1) * block_bytes;
                file.bytes.insert(file.bytes.end(), data, data + block_bytes);
            }
        }
    }

    return Status::Ok;
}

const std::vector<MemoryCard::File>& MemoryCard::files() const {
    return files_;
}

const std::string& MemoryCard::error() const {
    return error_;
}

}
