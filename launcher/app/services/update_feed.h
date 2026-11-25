#ifndef LOM_LAUNCHER_UPDATE_FEED_H
#define LOM_LAUNCHER_UPDATE_FEED_H

#include <QByteArray>
#include <QString>

#include <functional>

namespace lom::launcher {
class UpdateFeed {
  public:
    struct Response {
        bool ok = false;
        QByteArray body;
        QString error;
    };

    /**
     * @brief Destroy it.
     */
    virtual ~UpdateFeed() = default;

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
