#include "services/github_update_feed.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace lom::launcher {

namespace {

constexpr char latest_release_url[] = "https://api.github.com/repos/%1/releases/latest";
constexpr char releases_url[] = "https://api.github.com/repos/%1/releases?per_page=20";
constexpr char accept_header[] = "application/vnd.github+json";
constexpr char user_agent[] = "lom-native-launcher";
constexpr int timeout_ms = 10000;
constexpr qint64 max_response_bytes = 1024 * 1024;

}

GitHubUpdateFeed::GitHubUpdateFeed(QString repository, QObject* parent) : QObject(parent), repository_(std::move(repository)) {
}

void GitHubUpdateFeed::fetch_latest_release(std::function<void(const Response&)> done) {
    get(QUrl(QString::fromLatin1(latest_release_url).arg(repository_)), latest_reply_, std::move(done));
}

void GitHubUpdateFeed::fetch_releases(std::function<void(const Response&)> done) {
    get(QUrl(QString::fromLatin1(releases_url).arg(repository_)), releases_reply_, std::move(done));
}

void GitHubUpdateFeed::get(const QUrl& url, QNetworkReply*& reply, std::function<void(const Response&)> done) {
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
        } else if (running->error() == QNetworkReply::ContentNotFoundError) {
            response.error = QStringLiteral("GitHub couldn't find the releases of %1").arg(repository_);
        } else {
            response.error = running->errorString();
        }

        done(response);
    });
}

}
