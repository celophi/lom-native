#ifndef LOM_LAUNCHER_RANDOM_SOURCE_H
#define LOM_LAUNCHER_RANDOM_SOURCE_H

#include <cstdint>

namespace lom::launcher {
class RandomSource {
  public:
    /**
     * @brief Destroy it.
     */
    virtual ~RandomSource() = default;

    /**
     * @brief Draw a random number.
     * @return Any 32-bit number, equally likely.
     */
    virtual std::uint32_t next() = 0;

    /**
     * @brief Access the system random source.
     * @return Process-wide source seeded by the operating system.
     */
    static RandomSource& system();
};

}

#endif
