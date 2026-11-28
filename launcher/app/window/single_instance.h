#ifndef LOM_LAUNCHER_SINGLE_INSTANCE_H
#define LOM_LAUNCHER_SINGLE_INSTANCE_H

#include "services/file_lock_provider.h"
#include <QObject>
#include <QString>

#include <filesystem>
#include <memory>

class QLocalServer;

namespace lom::launcher {
class SingleInstance : public QObject {
    Q_OBJECT

  public:
    /**
     * @brief Prepare the guard for a per-user data directory.
     * @param data_directory Per-user launcher data directory.
     * @param locks What takes the launcher's lock; must outlive it.
     * @param parent Owner.
     */
    SingleInstance(const std::filesystem::path& data_directory, FileLockProvider& locks, QObject* parent);

    /**
     * @brief Release the guard.
     */
    ~SingleInstance() override;

    /**
     * @brief Become the primary launcher, or notify the existing one.
     * @return True when this process should continue.
     */
    bool acquire();

  signals:
    /**
     * @brief Another launcher asked this one to come to the front.
     */
    void activation_requested();

  private:
    FileLockProvider& locks_;
    std::filesystem::path lock_path_;
    std::unique_ptr<FileLockProvider::Lock> lock_;
    QLocalServer* server_ = nullptr;
    QString server_name_;
};

}

#endif
