#ifndef LOM_LAUNCHER_UPDATE_FEED_PROVIDER_H
#define LOM_LAUNCHER_UPDATE_FEED_PROVIDER_H

#include <QByteArray>
#include <QString>

#include <functional>

namespace lom::launcher {
class UpdateFeedProvider {
  public:
    struct Response {
        bool ok = false;
        bool not_found = false;
        QByteArray body;
        QString error;
    };

    /**
     * @brief Destroy it.
     */
    virtual ~UpdateFeedProvider() = default;

    /**
     * @brief Ask for the latest release, in the background.
     * @param done Called with the response, on the main thread.
     */
    virtual void fetch_latest_release(std::function<void(const Response&)> done) = 0;

    /**
     * @brief Ask for every release, in the background, for their notes.
     * @param done Called with the response, on the main thread.
     */
    virtual void fetch_releases(std::function<void(const Response&)> done) = 0;
};

}

#endif
