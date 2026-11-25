#include "lom/launcher/common/sha1.h"

#include "common/hex_text.h"

#include <cstring>

namespace lom::launcher {

namespace {

constexpr std::array<std::uint32_t, 5> initial_state = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
constexpr std::size_t block_bytes = 64;
constexpr std::size_t length_field_offset = 56;

/**
 * @brief Rotate a 32-bit word left.
 * @param value Word to rotate.
 * @param bits Rotation from 1 to 31.
 * @return Rotated word.
 */
std::uint32_t rotate_left(std::uint32_t value, int bits) {
    return (value << bits) | (value >> (32 - bits));
}

}

Sha1::Sha1() : state_(initial_state), buffer_{} {
}

void Sha1::update(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    total_bytes_ += size;

    while (size > 0) {
        std::size_t take = block_bytes - buffered_;

        if (take > size) {
            take = size;
        }

        std::memcpy(buffer_.data() + buffered_, bytes, take);
        buffered_ += take;
        bytes += take;
        size -= take;

        if (buffered_ == block_bytes) {
            transform(buffer_.data());
            buffered_ = 0;
        }
    }
}

std::string Sha1::hex_digest() {
    const std::uint64_t bit_length = total_bytes_ * 8;
    const std::uint8_t terminator = 0x80;
    update(&terminator, 1);

    const std::uint8_t zero = 0;

    while (buffered_ != length_field_offset) {
        update(&zero, 1);
    }

    std::uint8_t length[8];

    for (int i = 0; i < 8; i++) {
        length[i] = static_cast<std::uint8_t>(bit_length >> (56 - 8 * i));
    }

    update(length, sizeof(length));
    std::uint8_t digest[digest_bytes];

    for (std::size_t i = 0; i < state_.size(); i++) {
        digest[i * 4] = static_cast<std::uint8_t>(state_[i] >> 24);
        digest[i * 4 + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
        digest[i * 4 + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(state_[i]);
    }

    return hex_text(digest, digest_bytes);
}

void Sha1::transform(const std::uint8_t* block) {
    std::uint32_t w[80];

    for (int i = 0; i < 16; i++) {
        w[i] = (std::uint32_t(block[i * 4]) << 24) | (std::uint32_t(block[i * 4 + 1]) << 16) | (std::uint32_t(block[i * 4 + 2]) << 8) |
               std::uint32_t(block[i * 4 + 3]);
    }

    for (int i = 16; i < 80; i++) {
        w[i] = rotate_left(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];

    for (int i = 0; i < 80; i++) {
        std::uint32_t f;
        std::uint32_t k;

        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }

        const std::uint32_t next = rotate_left(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotate_left(b, 30);
        b = a;
        a = next;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
}

}
