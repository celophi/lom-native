#ifndef LOM_LAUNCHER_DOWNLOAD_PROVIDER_MOCK_H
#define LOM_LAUNCHER_DOWNLOAD_PROVIDER_MOCK_H

#include "services/download_provider.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class DownloadProviderMock : public DownloadProvider {
  public:
    MOCK_METHOD(void, get,
                (const QUrl& url, std::function<void(const QByteArray& piece)> received, std::function<void(qint64 received, qint64 total)> progress,
                 std::function<void(const Done& done)> done),
                (override));
    MOCK_METHOD(void, cancel, (), (override));
};

}

#endif
