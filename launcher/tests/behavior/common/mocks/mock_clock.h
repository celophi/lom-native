#ifndef LOM_LAUNCHER_MOCK_CLOCK_H
#define LOM_LAUNCHER_MOCK_CLOCK_H

#include "lom/launcher/common/clock.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class MockClock : public Clock {
  public:
    MOCK_METHOD(std::int64_t, now_unix_seconds, (), (const, override));
};

}

#endif
