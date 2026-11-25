#ifndef LOM_LAUNCHER_SHA1_H
#define LOM_LAUNCHER_SHA1_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace lom::launcher {
class Sha1 {
  public:
    static constexpr std::size_t digest_bytes = 20;

    /**
     * @brief Start an empty hash.
     */
    Sha1();

    /**
     * @brief Add bytes to the hash.
     * @param data Bytes to hash; may be null when size is zero.
     * @param size Number of bytes.
     */
    void update(const void* data, std::size_t size);

    /**
     * @brief Finish the hash and return the lowercase hexadecimal digest.
     * @return 40 hexadecimal characters. The object must not be updated afterwards.
     */
    std::string hex_digest();

  private:
    /**
     * @brief Mix one 64-byte block into the state.
     * @param block Block bytes.
     */
    void transform(const std::uint8_t* block);

    std::array<std::uint32_t, 5> state_;
    std::array<std::uint8_t, 64> buffer_;
    std::size_t buffered_ = 0;
    std::uint64_t total_bytes_ = 0;
};

}

#endif
