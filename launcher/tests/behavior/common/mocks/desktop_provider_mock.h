#ifndef LOM_LAUNCHER_MOCK_DESKTOP_PROVIDER_H
#define LOM_LAUNCHER_MOCK_DESKTOP_PROVIDER_H

#include "services/desktop_provider.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class DesktopProviderMock : public DesktopProvider {
  public:
    MOCK_METHOD(void, open_folder, (const QString& folder), (override));
    MOCK_METHOD(void, open_url, (const QUrl& url), (override));
};

}

#endif
