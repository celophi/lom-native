#include "common/hex_text.h"

namespace lom::launcher {

namespace {

constexpr char hex_digits[] = "0123456789abcdef";

}

std::string hex_text(const std::uint8_t* bytes, std::size_t size) {
    std::string text;
    text.reserve(size * 2);

    for (std::size_t i = 0; i < size; i++) {
        text.push_back(hex_digits[bytes[i] >> 4]);
        text.push_back(hex_digits[bytes[i] & 15]);
    }

    return text;
}

}
