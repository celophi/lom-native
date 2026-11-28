#ifndef LOM_LAUNCHER_MOCK_FILE_LOCK_PROVIDER_H
#define LOM_LAUNCHER_MOCK_FILE_LOCK_PROVIDER_H

#include "services/file_lock_provider.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class FileLockProviderMock : public FileLockProvider {
  public:
    MOCK_METHOD(std::unique_ptr<Lock>, try_lock, (const std::filesystem::path& path), (override));
};

}

#endif
