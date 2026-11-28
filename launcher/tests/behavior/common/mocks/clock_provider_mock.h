#ifndef LOM_LAUNCHER_MOCK_CLOCK_PROVIDER_H
#define LOM_LAUNCHER_MOCK_CLOCK_PROVIDER_H

#include "lom/launcher/common/clock_provider.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class ClockProviderMock : public ClockProvider {
  public:
    MOCK_METHOD(std::int64_t, now_unix_seconds, (), (const, override));
};

}

#endif
