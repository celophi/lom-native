#include "synthetic_card.h"

#include "lom/launcher/saves/memory_card.h"
#include "synthetic_disc.h"

#include <algorithm>

namespace lom::launcher::test {

namespace {
constexpr std::size_t save_bytes = 0x4000;

}

std::vector<std::uint8_t> SyntheticCard::save(std::uint32_t seed) {
    std::vector<std::uint8_t> save = SyntheticDisc::pattern(save_bytes, seed);
    save[0] = 'S';
    save[1] = 'C';
    std::uint32_t sum = 0;

    for (std::size_t i = 0; i < 0x33E0; i++) {
        sum += save[i];
    }

    const std::uint32_t checksum = sum * 2 + 0x0414E410u;
    const std::uint32_t magic = 0x00414E41u;

    for (int i = 0; i < 4; i++) {
        save[0x33E0 + i] = std::uint8_t(checksum >> (8 * i));
        save[0x33E4 + i] = std::uint8_t(magic >> (8 * i));
    }

    return save;
}

SyntheticCard::SyntheticCard() : bytes_(MemoryCard::card_bytes, 0) {
    bytes_[0] = 'M';
    bytes_[1] = 'C';
    seal(0);

    for (int block = 0; block < MemoryCard::data_blocks; block++) {
        frame(block)[0] = 0xA0;
        frame(block)[8] = 0xFF;
        frame(block)[9] = 0xFF;
        seal(block + 1);
    }
}

void SyntheticCard::add(const std::string& name, const std::vector<int>& blocks, const std::vector<std::uint8_t>& data) {
    for (std::size_t i = 0; i < blocks.size(); i++) {
        std::uint8_t* entry = frame(blocks[i]);
        std::fill_n(entry, MemoryCard::frame_bytes, 0);
        entry[0] = i == 0 ? 0x51 : i + 1 == blocks.size() ? 0x53 : 0x52;

        if (i == 0) {
            const std::uint32_t size = std::uint32_t(blocks.size() * MemoryCard::block_bytes);

            for (int b = 0; b < 4; b++) {
                entry[4 + b] = std::uint8_t(size >> (8 * b));
            }

            std::copy(name.begin(), name.end(), entry + 0x0A);
        }

        const int next = i + 1 < blocks.size() ? blocks[i + 1] : 0xFFFF;
        entry[8] = std::uint8_t(next);
        entry[9] = std::uint8_t(next >> 8);
        seal(blocks[i] + 1);
        std::copy_n(data.begin() + std::ptrdiff_t(i * MemoryCard::block_bytes), MemoryCard::block_bytes,
                    bytes_.begin() + std::ptrdiff_t((blocks[i] + 1) * MemoryCard::block_bytes));
    }
}

std::uint8_t* SyntheticCard::frame(int block) {
    return bytes_.data() + std::size_t(block + 1) * MemoryCard::frame_bytes;
}

void SyntheticCard::seal(int index) {
    std::uint8_t* data = bytes_.data() + std::size_t(index) * MemoryCard::frame_bytes;
    std::uint8_t value = 0;

    for (std::size_t i = 0; i < MemoryCard::frame_bytes - 1; i++) {
        value ^= data[i];
    }

    data[MemoryCard::frame_bytes - 1] = value;
}

const std::vector<std::uint8_t>& SyntheticCard::bytes() const {
    return bytes_;
}

}
