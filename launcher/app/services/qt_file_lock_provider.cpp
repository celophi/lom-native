#include "services/qt_file_lock_provider.h"

#include "window/app_paths.h"

#include <QLockFile>

namespace lom::launcher {

namespace {

class QtLock : public FileLockProvider::Lock {
  public:
    /**
     * @brief Hold a lock Qt took.
     * @param file The lock.
     */
    explicit QtLock(std::unique_ptr<QLockFile> file) : file_(std::move(file)) {
    }

  private:
    std::unique_ptr<QLockFile> file_;
};

}

std::unique_ptr<FileLockProvider::Lock> QtFileLockProvider::try_lock(const std::filesystem::path& path) {
    auto file = std::make_unique<QLockFile>(AppPaths::to_text(path));

    if (!file->tryLock(0)) {
        return nullptr;
    }

    return std::make_unique<QtLock>(std::move(file));
}

}
