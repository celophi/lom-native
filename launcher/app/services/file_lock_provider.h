#ifndef LOM_LAUNCHER_FILE_LOCK_PROVIDER_H
#define LOM_LAUNCHER_FILE_LOCK_PROVIDER_H

#include <filesystem>
#include <memory>

namespace lom::launcher {
class FileLockProvider {
  public:
    class Lock {
      public:
        /**
         * @brief Let the lock go.
         */
        virtual ~Lock() = default;
    };

    /**
     * @brief Destroy it.
     */
    virtual ~FileLockProvider() = default;

    /**
     * @brief Take the system's lock on a file, without waiting, as only one program can at a time.
     * @param path The lock's file.
     * @return The lock, held until it's gone, or nullptr when another program has it.
     * @note The launcher's provider for locking files: the one place it does, as thin as it can be.
     */
    virtual std::unique_ptr<Lock> try_lock(const std::filesystem::path& path) = 0;
};

}

#endif
