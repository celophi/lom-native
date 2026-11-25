#ifndef LOM_LAUNCHER_MOCK_DESKTOP_H
#define LOM_LAUNCHER_MOCK_DESKTOP_H

#include "services/desktop.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class MockDesktop : public Desktop {
  public:
    MOCK_METHOD(void, open_folder, (const QString& folder), (override));
    MOCK_METHOD(void, open_url, (const QUrl& url), (override));
};

}

#endif
