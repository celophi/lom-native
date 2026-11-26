#ifndef LOM_LAUNCHER_SYSTEM_UPDATE_INSTALLER_H
#define LOM_LAUNCHER_SYSTEM_UPDATE_INSTALLER_H

#include "lom/launcher/common/update_package.h"
#include "services/update_installer.h"

#include <QCryptographicHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include <filesystem>
#include <optional>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;
class QThread;

namespace lom::launcher {
class SystemUpdateInstaller : public QObject, public UpdateInstaller {
    Q_OBJECT

  public:
    struct Target {
        UpdatePackage::Kind kind = UpdatePackage::Kind::AppImage;
        std::filesystem::path path;
        std::filesystem::path staging;
        QString program;
        QStringList arguments;
    };

    /**
     * @brief Work out what this copy of the launcher would update: its AppImage,
     *        its Windows folder or its Mac app.
     * @param parent Owner.
     */
    explicit SystemUpdateInstaller(QObject* parent);

    /**
     * @brief Update a given target instead of this copy of the launcher, for tests.
     * @param target What to update, or nothing for an installer that can't.
     * @param parent Owner.
     */
    SystemUpdateInstaller(std::optional<Target> target, QObject* parent);

    /**
     * @brief Stop a running download.
     */
    ~SystemUpdateInstaller() override;

    /**
     * @brief Find what this copy of the launcher would update, if it can update itself.
     * @return It, or nothing for a Flatpak, a build from source, an app macOS is
     *         running from a read-only place, or a folder the launcher can't write to.
     */
    static std::optional<Target> detect();

    QString package_name(const QString& version) const override;

    void install(const Request& request, std::function<void(const Progress&)> progress, std::function<void(const Outcome&)> done) override;

    void cancel() override;

    bool restart() override;

    void clean_up() override;

  private:
    /**
     * @brief Download the package once its checksum is known.
     * @param digest Its SHA-256 from SHA256SUMS.
     */
    void download_package(const QString& digest);

    /**
     * @brief Put the checked package in place on another thread.
     */
    void put_in_place();

    /**
     * @brief Unpack the package and swap it in; runs on the worker thread.
     * @param target What gets replaced.
     * @param package The downloaded file.
     * @return Why it failed, or empty.
     */
    static QString apply(const Target& target, const std::filesystem::path& package);

    /**
     * @brief End the install and report it.
     * @param outcome How it went.
     */
    void finish(const Outcome& outcome);

    std::optional<Target> target_;
    QNetworkAccessManager* network_ = nullptr;
    QNetworkReply* reply_ = nullptr;
    QFile* file_ = nullptr;
    QThread* worker_ = nullptr;
    QString write_error_;
    QCryptographicHash hash_{QCryptographicHash::Sha256};
    Request request_;
    std::filesystem::path package_path_;
    std::function<void(const Progress&)> progress_;
    std::function<void(const Outcome&)> done_;
    bool busy_ = false;
    bool cancelled_ = false;
};

}

#endif
