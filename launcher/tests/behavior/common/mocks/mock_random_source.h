#ifndef LOM_LAUNCHER_MOCK_RANDOM_SOURCE_H
#define LOM_LAUNCHER_MOCK_RANDOM_SOURCE_H

#include "lom/launcher/common/random_source.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class MockRandomSource : public RandomSource {
  public:
    MOCK_METHOD(std::uint32_t, next, (), (override));
};

}

#endif
