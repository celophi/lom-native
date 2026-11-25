#ifndef LOM_LAUNCHER_CLOCK_H
#define LOM_LAUNCHER_CLOCK_H

#include <cstdint>

namespace lom::launcher {
class Clock {
  public:
    /**
     * @brief Destroy it.
     */
    virtual ~Clock() = default;

    /**
     * @brief Read the time.
     * @return Seconds since 1970, in UTC.
     */
    virtual std::int64_t now_unix_seconds() const = 0;

    /**
     * @brief Access the system clock.
     * @return Process-wide clock reading the real time.
     */
    static const Clock& system();
};

}

#endif
