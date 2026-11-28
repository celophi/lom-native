#include "services/github_update_feed_provider.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>

#include <optional>

namespace {

using namespace lom::launcher;

constexpr int timeout_ms = 10000;

class UpdateFeedProviderContract : public ::testing::Test {
  protected:
    /**
     * @brief Serve one HTTP reply on this machine, standing in for GitHub's API, and note the request it answers.
     */
    UpdateFeedProviderContract() {
        server.listen(QHostAddress::LocalHost);
        QObject::connect(&server, &QTcpServer::newConnection, [this]() {
            QTcpSocket* socket = server.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, [this, socket]() {
                request += socket->readAll();

                if (!request.contains("\r\n\r\n")) {
                    return;
                }

                socket->write("HTTP/1.1 " + status + "\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(body.size()) +
                              "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
    }

    /**
     * @brief Where the stand-in API is.
     * @return Its address.
     */
    QString api_root() const {
        return QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    }

    /**
     * @brief Wait for the feed's answer, running the event loop.
     * @param fetch Starts the fetch, handing it the callback.
     * @return The answer, or nothing when it didn't come in time.
     */
    std::optional<UpdateFeedProvider::Response> answer(const std::function<void(std::function<void(const UpdateFeedProvider::Response&)>)>& fetch) {
        std::optional<UpdateFeedProvider::Response> response;
        fetch([&response](const UpdateFeedProvider::Response& done) {
            response = done;
        });
        QElapsedTimer waited;
        waited.start();

        while (!response && waited.elapsed() < timeout_ms) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }

        return response;
    }

    QTcpServer server;
    QByteArray status = "200 OK";
    QByteArray body = R"({"tag_name":"v1.4.0"})";
    QByteArray request;
};

TEST_F(UpdateFeedProviderContract, asks_githubs_api_for_the_latest_release_and_hands_back_its_body) {
    GitHubUpdateFeedProvider feed(QStringLiteral("celophi/lom-native"), nullptr, api_root());

    const auto response = answer([&feed](auto done) { feed.fetch_latest_release(done); });

    ASSERT_TRUE(response.has_value());
    EXPECT_TRUE(response->ok);
    EXPECT_EQ(response->body, QByteArray(R"({"tag_name":"v1.4.0"})"));
    EXPECT_TRUE(request.startsWith("GET /repos/celophi/lom-native/releases/latest HTTP/1.1"));
    EXPECT_TRUE(request.contains("Accept: application/vnd.github+json"));
    EXPECT_TRUE(request.contains("User-Agent: lom-native-launcher"));
}

TEST_F(UpdateFeedProviderContract, asks_for_the_last_twenty_releases) {
    body = "[]";
    GitHubUpdateFeedProvider feed(QStringLiteral("celophi/lom-native"), nullptr, api_root());

    const auto response = answer([&feed](auto done) { feed.fetch_releases(done); });

    ASSERT_TRUE(response.has_value());
    EXPECT_TRUE(response->ok);
    EXPECT_TRUE(request.startsWith("GET /repos/celophi/lom-native/releases?per_page=20 HTTP/1.1"));
}

TEST_F(UpdateFeedProviderContract, says_when_the_releases_are_not_found) {
    status = "404 Not Found";
    body = R"({"message":"Not Found"})";
    GitHubUpdateFeedProvider feed(QStringLiteral("celophi/missing"), nullptr, api_root());

    const auto response = answer([&feed](auto done) { feed.fetch_latest_release(done); });

    ASSERT_TRUE(response.has_value());
    EXPECT_FALSE(response->ok);
    EXPECT_TRUE(response->not_found);
    EXPECT_FALSE(response->error.isEmpty());
}

TEST_F(UpdateFeedProviderContract, gives_the_error_when_the_server_fails) {
    status = "500 Internal Server Error";
    GitHubUpdateFeedProvider feed(QStringLiteral("celophi/lom-native"), nullptr, api_root());

    const auto response = answer([&feed](auto done) { feed.fetch_latest_release(done); });

    ASSERT_TRUE(response.has_value());
    EXPECT_FALSE(response->ok);
    EXPECT_FALSE(response->not_found);
    EXPECT_FALSE(response->error.isEmpty());
}

}
