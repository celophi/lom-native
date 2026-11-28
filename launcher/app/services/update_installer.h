#ifndef LOM_LAUNCHER_UPDATE_INSTALLER_H
#define LOM_LAUNCHER_UPDATE_INSTALLER_H

#include "lom/launcher/common/update_package.h"
#include "services/download_provider.h"
#include "services/tools_provider.h"

#include <QCryptographicHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtGlobal>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>

class QThread;

namespace lom::launcher {
class UpdateInstaller : public QObject {
    Q_OBJECT

  public:
    static constexpr char after_update_argument[] = "--after-update";

    struct Progress {
        qint64 received = 0;
        qint64 total = 0;
        bool installing = false;
    };

    struct Outcome {
        bool ok = false;
        bool cancelled = false;
        QString error;
    };

    struct Request {
        QString version;
        QUrl package;
        QUrl checksums;
    };

    struct Target {
        UpdatePackage::Kind kind = UpdatePackage::Kind::AppImage;
        std::filesystem::path path;
        std::filesystem::path staging;
        std::filesystem::path tools;
        QString program;
        QStringList arguments;
    };

    /**
     * @brief An installer for a target: this copy of the launcher, or a test's.
     * @param target What it updates (its AppImage, Windows folder or Mac app, where the system's tools are, and how
     *        to start it again), or nothing for a copy that can't update itself.
     * @param downloads Fetches the release files.
     * @param tools Runs the system's unpacking tools and starts the new version.
     * @param parent Owner.
     */
    UpdateInstaller(std::optional<Target> target, DownloadProvider& downloads, ToolsProvider& tools, QObject* parent);

    /**
     * @brief Stop a running download and wait for the new version to finish going in.
     */
    ~UpdateInstaller() override;

    /**
     * @brief Find what this copy of the launcher would update, if it can update itself.
     * @return It, or nothing for a Flatpak, a build from source, an app macOS is
     *         running from a read-only place, or a folder the launcher can't write to.
     */
    static std::optional<Target> detect();

    /**
     * @brief Name the release file this copy of the launcher updates itself from.
     * @param version The release's version, like "1.0.1".
     * @return The file's name, or empty when this copy can't update itself.
     */
    QString package_name(const QString& version) const;

    /**
     * @brief Download a release, check it against its SHA256SUMS and put it in place, in the background.
     * @param request What to download.
     * @param progress Called on the main thread as it goes.
     * @param done Called once on the main thread when it's finished, failed or cancelled.
     */
    void install(const Request& request, std::function<void(const Progress&)> progress, std::function<void(const Outcome&)> done);

    /**
     * @brief Stop a download. Once the new version is going in, it finishes.
     */
    void cancel();

    /**
     * @brief Start the newly installed launcher, for the caller to quit after.
     * @return False when it couldn't be started.
     */
    bool restart();

    /**
     * @brief Delete what an earlier update left behind.
     */
    void clean_up();

    /**
     * @brief Wait for a new version that's going in to finish going in. Its end is reported once events are handled.
     */
    void wait_for_work();

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
     * @param tools Runs the system's tools.
     * @return Why it failed, or empty.
     */
    static QString apply(const Target& target, const std::filesystem::path& package, ToolsProvider& tools);

    /**
     * @brief End the install and report it.
     * @param outcome How it went.
     */
    void finish(const Outcome& outcome);

    std::optional<Target> target_;
    DownloadProvider& downloads_;
    ToolsProvider& tools_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    std::unique_ptr<std::ostream> file_;
    QByteArray checksums_;
    QThread* worker_ = nullptr;
    QString write_error_;
    QCryptographicHash hash_{QCryptographicHash::Sha256};
    Request request_;
    std::filesystem::path package_path_;
    std::function<void(const Progress&)> progress_;
    std::function<void(const Outcome&)> done_;
    bool busy_ = false;
    bool downloading_ = false;
    bool cancelled_ = false;
};

}

#endif
