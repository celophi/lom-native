#ifndef LOM_LAUNCHER_MOCK_RANDOM_PROVIDER_H
#define LOM_LAUNCHER_MOCK_RANDOM_PROVIDER_H

#include "lom/launcher/common/random_provider.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class RandomProviderMock : public RandomProvider {
  public:
    MOCK_METHOD(std::uint32_t, next, (), (override));
};

}

#endif
