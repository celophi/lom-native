#include "lom/launcher/common/clock.h"

#include <chrono>

namespace lom::launcher {

namespace {
class SystemClock : public Clock {
  public:
    std::int64_t now_unix_seconds() const override {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

}

const Clock& Clock::system() {
    static const SystemClock clock;
    return clock;
}

}
