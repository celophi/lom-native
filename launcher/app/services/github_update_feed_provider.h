#ifndef LOM_LAUNCHER_GITHUB_UPDATE_FEED_PROVIDER_H
#define LOM_LAUNCHER_GITHUB_UPDATE_FEED_PROVIDER_H

#include "services/update_feed_provider.h"

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QUrl;

namespace lom::launcher {
class GitHubUpdateFeedProvider : public QObject, public UpdateFeedProvider {
    Q_OBJECT

  public:
    /**
     * @brief Create a feed for a repository.
     * @param repository GitHub "owner/name".
     * @param parent Owner.
     * @param api_root Where GitHub's API is; another address only for the provider's contract test.
     */
    GitHubUpdateFeedProvider(QString repository, QObject* parent, QString api_root = QStringLiteral("https://api.github.com"));

    /**
     * @brief Ask GitHub for the latest stable release; ignored while a request is running.
     * @param done Called once with the response.
     */
    void fetch_latest_release(std::function<void(const Response&)> done) override;

    /**
     * @brief Ask GitHub for the recent releases; ignored while a request for them is running.
     * @param done Called once with the response.
     */
    void fetch_releases(std::function<void(const Response&)> done) override;

  private:
    /**
     * @brief Start a GET request and answer once it ends.
     * @param url Address.
     * @param reply Where the running request is kept, so a second one is ignored.
     * @param done Called once with the response.
     */
    void get(const QUrl& url, QNetworkReply*& reply, std::function<void(const Response&)> done);

    QString repository_;
    QString api_root_;
    QNetworkAccessManager* network_ = nullptr;
    QNetworkReply* latest_reply_ = nullptr;
    QNetworkReply* releases_reply_ = nullptr;
};

}

#endif
