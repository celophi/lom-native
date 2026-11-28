#include "services/qt_download_provider.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace lom::launcher {

namespace {

constexpr char user_agent[] = "lom-native-launcher";
constexpr int transfer_timeout_ms = 30000;

}

QtDownloadProvider::QtDownloadProvider(QObject* parent) : QObject(parent) {
}

QtDownloadProvider::~QtDownloadProvider() {
    if (reply_ != nullptr) {
        reply_->disconnect(this);
        reply_->abort();
    }
}

void QtDownloadProvider::get(const QUrl& url, std::function<void(const QByteArray& piece)> received, std::function<void(qint64 received, qint64 total)> progress,
                             std::function<void(const Done& done)> done) {
    if (network_ == nullptr) {
        network_ = new QNetworkAccessManager(this);
    }

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", user_agent);
    request.setTransferTimeout(transfer_timeout_ms);
    cancelled_ = false;
    QNetworkReply* reply = network_->get(request);
    reply_ = reply;
    connect(reply, &QNetworkReply::readyRead, this, [reply, received]() {
        received(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [progress](qint64 so_far, qint64 total) {
        progress(so_far, total < 0 ? 0 : total);
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, received, done]() {
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray rest = reply->readAll();

            if (!rest.isEmpty()) {
                received(rest);
            }
        }

        reply_ = nullptr;
        reply->deleteLater();
        Done result;
        result.cancelled = cancelled_;
        result.ok = !cancelled_ && reply->error() == QNetworkReply::NoError;
        result.error = result.ok || cancelled_ ? QString() : reply->errorString();
        done(result);
    });
}

void QtDownloadProvider::cancel() {
    if (reply_ != nullptr) {
        cancelled_ = true;
        reply_->abort();
    }
}

}
