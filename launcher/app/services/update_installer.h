#ifndef LOM_LAUNCHER_UPDATE_INSTALLER_H
#define LOM_LAUNCHER_UPDATE_INSTALLER_H

#include <QString>
#include <QUrl>
#include <QtGlobal>

#include <functional>

namespace lom::launcher {
class UpdateInstaller {
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

    /**
     * @brief Destroy it.
     */
    virtual ~UpdateInstaller() = default;

    /**
     * @brief Name the release file this copy of the launcher updates itself from.
     * @param version The release's version, like "1.0.1".
     * @return The file's name, or empty when this copy can't update itself (a
     *         Flatpak, a build from source, or a folder it can't write to).
     */
    virtual QString package_name(const QString& version) const = 0;

    /**
     * @brief Download a release, check it against its SHA256SUMS and put it in place, in the background.
     * @param request What to download.
     * @param progress Called on the main thread as it goes.
     * @param done Called once on the main thread when it's finished, failed or cancelled.
     */
    virtual void install(const Request& request, std::function<void(const Progress&)> progress, std::function<void(const Outcome&)> done) = 0;

    /**
     * @brief Stop a download. Once the new version is going in, it finishes.
     */
    virtual void cancel() = 0;

    /**
     * @brief Start the newly installed launcher, for the caller to quit after.
     * @return False when it couldn't be started.
     */
    virtual bool restart() = 0;

    /**
     * @brief Delete what an earlier update left behind.
     */
    virtual void clean_up() = 0;
};

}

#endif
