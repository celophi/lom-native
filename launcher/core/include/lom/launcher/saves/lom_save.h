#ifndef LOM_LAUNCHER_LOM_SAVE_H
#define LOM_LAUNCHER_LOM_SAVE_H

#include <cstddef>
#include <cstdint>

namespace lom::launcher {
class LomSave {
  public:
    enum class Status { Valid, WrongSize, BadHeader, BadChecksum, BadMagic };

    static constexpr std::size_t file_bytes = 0x4000;

    /**
     * @brief Validate a save file's size, card header, checksum and magic.
     * @param data File bytes.
     * @param size Number of bytes.
     * @return Valid, or the first failed check.
     */
    static Status validate(const std::uint8_t* data, std::size_t size);
};

}

#endif
