#include "lom/launcher/common/clock_provider.h"

#include <chrono>

namespace lom::launcher {

namespace {
class SystemClockProvider : public ClockProvider {
  public:
    std::int64_t now_unix_seconds() const override {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

}

const ClockProvider& ClockProvider::system() {
    static const SystemClockProvider clock;
    return clock;
}

}
