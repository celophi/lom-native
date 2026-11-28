#ifndef LOM_LAUNCHER_QT_FILE_LOCK_PROVIDER_H
#define LOM_LAUNCHER_QT_FILE_LOCK_PROVIDER_H

#include "services/file_lock_provider.h"

namespace lom::launcher {
class QtFileLockProvider : public FileLockProvider {
  public:
    std::unique_ptr<Lock> try_lock(const std::filesystem::path& path) override;
};

}

#endif
