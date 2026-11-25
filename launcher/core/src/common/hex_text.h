#ifndef LOM_LAUNCHER_HEX_TEXT_H
#define LOM_LAUNCHER_HEX_TEXT_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace lom::launcher {

/**
 * @brief Format bytes as lowercase hexadecimal text.
 * @param bytes Bytes to format.
 * @param size Number of bytes.
 * @return Two characters per byte.
 */
std::string hex_text(const std::uint8_t* bytes, std::size_t size);

}

#endif
