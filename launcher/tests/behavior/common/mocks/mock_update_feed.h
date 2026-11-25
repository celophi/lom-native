#ifndef LOM_LAUNCHER_MOCK_UPDATE_FEED_H
#define LOM_LAUNCHER_MOCK_UPDATE_FEED_H

#include "services/update_feed.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class MockUpdateFeed : public UpdateFeed {
  public:
    MOCK_METHOD(void, fetch_latest_release, (std::function<void(const Response&)> done), (override));
    MOCK_METHOD(void, fetch_releases, (std::function<void(const Response&)> done), (override));
};

}

#endif
