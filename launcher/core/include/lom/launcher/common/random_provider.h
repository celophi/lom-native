#ifndef LOM_LAUNCHER_RANDOM_PROVIDER_H
#define LOM_LAUNCHER_RANDOM_PROVIDER_H

#include <cstdint>

namespace lom::launcher {
class RandomProvider {
  public:
    /**
     * @brief Destroy it.
     */
    virtual ~RandomProvider() = default;

    /**
     * @brief Draw a random number.
     * @return Any 32-bit number, equally likely.
     */
    virtual std::uint32_t next() = 0;

    /**
     * @brief Access the system random source.
     * @return Process-wide source seeded by the operating system.
     */
    static RandomProvider& system();
};

}

#endif
