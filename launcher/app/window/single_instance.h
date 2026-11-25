#ifndef LOM_LAUNCHER_SINGLE_INSTANCE_H
#define LOM_LAUNCHER_SINGLE_INSTANCE_H

#include <QObject>
#include <QString>

#include <filesystem>
#include <memory>

class QLocalServer;
class QLockFile;

namespace lom::launcher {
class SingleInstance : public QObject {
    Q_OBJECT

  public:
    /**
     * @brief Prepare the guard for a per-user data directory.
     * @param data_directory Per-user launcher data directory.
     * @param parent Owner.
     */
    SingleInstance(const std::filesystem::path& data_directory, QObject* parent);

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
    std::unique_ptr<QLockFile> lock_;
    QLocalServer* server_ = nullptr;
    QString server_name_;
};

}

#endif
