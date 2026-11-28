#include "services/github_update_feed_provider.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace lom::launcher {

namespace {

constexpr char latest_release_url[] = "%1/repos/%2/releases/latest";
constexpr char releases_url[] = "%1/repos/%2/releases?per_page=20";
constexpr char accept_header[] = "application/vnd.github+json";
constexpr char user_agent[] = "lom-native-launcher";
constexpr int timeout_ms = 10000;
constexpr qint64 max_response_bytes = 1024 * 1024;

}

GitHubUpdateFeedProvider::GitHubUpdateFeedProvider(QString repository, QObject* parent, QString api_root)
    : QObject(parent), repository_(std::move(repository)), api_root_(std::move(api_root)) {
}

void GitHubUpdateFeedProvider::fetch_latest_release(std::function<void(const Response&)> done) {
    get(QUrl(QString::fromLatin1(latest_release_url).arg(api_root_, repository_)), latest_reply_, std::move(done));
}

void GitHubUpdateFeedProvider::fetch_releases(std::function<void(const Response&)> done) {
    get(QUrl(QString::fromLatin1(releases_url).arg(api_root_, repository_)), releases_reply_, std::move(done));
}

void GitHubUpdateFeedProvider::get(const QUrl& url, QNetworkReply*& reply, std::function<void(const Response&)> done) {
    if (reply != nullptr) {
        return;
    }

    if (network_ == nullptr) {
        network_ = new QNetworkAccessManager(this);
    }

    QNetworkRequest request(url);
    request.setRawHeader("Accept", accept_header);
    request.setRawHeader("User-Agent", user_agent);
    request.setTransferTimeout(timeout_ms);
    QNetworkReply* running = network_->get(request);
    reply = running;

    connect(running, &QNetworkReply::downloadProgress, this, [running](qint64 received, qint64) {
        if (received > max_response_bytes) {
            running->abort();
        }
    });

    connect(running, &QNetworkReply::finished, this, [this, running, &reply, done = std::move(done)]() {
        reply = nullptr;
        running->deleteLater();
        Response response;
        response.ok = running->error() == QNetworkReply::NoError;

        if (response.ok) {
            response.body = running->read(max_response_bytes);
        } else {
            response.not_found = running->error() == QNetworkReply::ContentNotFoundError;
            response.error = running->errorString();
        }

        done(response);
    });
}

}
