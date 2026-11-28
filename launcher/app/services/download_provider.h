#ifndef LOM_LAUNCHER_DOWNLOAD_PROVIDER_H
#define LOM_LAUNCHER_DOWNLOAD_PROVIDER_H

#include <QByteArray>
#include <QString>
#include <QUrl>

#include <functional>

namespace lom::launcher {
class DownloadProvider {
  public:
    struct Done {
        bool ok = false;
        bool cancelled = false;
        QString error;
    };

    /**
     * @brief Destroy it, stopping a download in progress.
     */
    virtual ~DownloadProvider() = default;

    /**
     * @brief Download a URL, handing over its bytes as they arrive. One at a time.
     * @param url What to download.
     * @param received Called with each piece, in order.
     * @param progress Called with how much has come and how much there is (0 when it isn't known yet).
     * @param done Called once at the end: whether it all came, or why not.
     * @note The launcher's provider for downloads: the one place it fetches a file over the network, as thin as it
     *       can be. What's done with the bytes (checking, writing, limits) is the launcher's.
     */
    virtual void get(const QUrl& url, std::function<void(const QByteArray& piece)> received, std::function<void(qint64 received, qint64 total)> progress,
                     std::function<void(const Done& done)> done) = 0;

    /**
     * @brief Stop the download in progress, which then ends as cancelled. Nothing when there's none.
     */
    virtual void cancel() = 0;
};

}

#endif
