#include "services/qt_download_provider.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>

#include <optional>
#include <utility>
#include <vector>

namespace {

using namespace lom::launcher;

constexpr int timeout_ms = 10000;

class DownloadProviderContract : public ::testing::Test {
  protected:
    /**
     * @brief Serve one HTTP reply on this machine, standing in for GitHub's release files, and note the request it answers.
     */
    DownloadProviderContract() {
        server.listen(QHostAddress::LocalHost);
        QObject::connect(&server, &QTcpServer::newConnection, [this]() {
            QTcpSocket* socket = server.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, [this, socket]() {
                request += socket->readAll();

                if (!request.contains("\r\n\r\n")) {
                    return;
                }

                asked = true;

                if (!answers) {
                    return;
                }

                socket->write("HTTP/1.1 " + status + "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
    }

    /**
     * @brief Where a file on the stand-in server is.
     * @return Its address.
     */
    QUrl url() const {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/releases/download/v1.4.0/SHA256SUMS").arg(server.serverPort()));
    }

    /**
     * @brief Start a download, noting what it hands over.
     */
    void download() {
        downloads.get(
            url(),
            [this](const QByteArray& piece) {
                received += piece;
            },
            [this](qint64 so_far, qint64 total) {
                progress.emplace_back(so_far, total);
            },
            [this](const DownloadProvider::Done& result) {
                done = result;
            });
    }

    /**
     * @brief Run the event loop until a condition holds.
     * @param condition The condition.
     * @return Whether it came true in time.
     */
    static bool wait_until(const std::function<bool()>& condition) {
        QElapsedTimer waited;
        waited.start();

        while (!condition() && waited.elapsed() < timeout_ms) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }

        return condition();
    }

    QtDownloadProvider downloads{nullptr};
    QTcpServer server;
    QByteArray status = "200 OK";
    QByteArray body = "0123456789abcdef  lom-native-1.4.0-linux-x86_64.AppImage\n";
    bool answers = true;
    bool asked = false;
    QByteArray request;
    QByteArray received;
    std::vector<std::pair<qint64, qint64>> progress;
    std::optional<DownloadProvider::Done> done;
};

}

TEST_F(DownloadProviderContract, hands_over_the_whole_file_and_says_it_came) {
    download();

    ASSERT_TRUE(wait_until([this]() { return done.has_value(); }));
    EXPECT_TRUE(done->ok);
    EXPECT_FALSE(done->cancelled);
    EXPECT_EQ(done->error, QString());
    EXPECT_EQ(received, body);
}

TEST_F(DownloadProviderContract, asks_for_the_file_as_the_launcher) {
    download();

    ASSERT_TRUE(wait_until([this]() { return done.has_value(); }));
    EXPECT_TRUE(request.startsWith("GET /releases/download/v1.4.0/SHA256SUMS HTTP/1.1\r\n"));
    EXPECT_TRUE(request.contains("\r\nUser-Agent: lom-native-launcher\r\n"));
}

TEST_F(DownloadProviderContract, reports_progress_up_to_the_whole_size) {
    download();

    ASSERT_TRUE(wait_until([this]() { return done.has_value(); }));
    ASSERT_FALSE(progress.empty());
    EXPECT_EQ(progress.back(), std::make_pair(qint64(body.size()), qint64(body.size())));
}

TEST_F(DownloadProviderContract, says_why_when_the_server_has_no_such_file) {
    status = "404 Not Found";
    body = "Not Found";

    download();

    ASSERT_TRUE(wait_until([this]() { return done.has_value(); }));
    EXPECT_FALSE(done->ok);
    EXPECT_FALSE(done->cancelled);
    EXPECT_FALSE(done->error.isEmpty());
}

TEST_F(DownloadProviderContract, ends_as_cancelled_when_cancelled) {
    answers = false;
    download();
    ASSERT_TRUE(wait_until([this]() { return asked; }));

    downloads.cancel();

    ASSERT_TRUE(wait_until([this]() { return done.has_value(); }));
    EXPECT_FALSE(done->ok);
    EXPECT_TRUE(done->cancelled);
}

TEST_F(DownloadProviderContract, cancelling_with_nothing_downloading_does_nothing) {
    downloads.cancel();

    download();

    ASSERT_TRUE(wait_until([this]() { return done.has_value(); }));
    EXPECT_TRUE(done->ok);
    EXPECT_EQ(received, body);
}
