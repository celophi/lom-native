#include "services/system_update_installer.h"

#include "lom/launcher/common/sha256.h"
#include "lom/launcher/common/zip_writer.h"
#include "window/app_paths.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>

#include <ctime>
#include <optional>

namespace {

using namespace lom::launcher;

constexpr int timeout_ms = 30000;
constexpr int poll_ms = 5;
constexpr char version[] = "9.9.9";

class FileServer {
  public:
    /**
     * @brief Start a web server on this computer, on a free port, that answers
     *        each path with fixed bytes.
     */
    FileServer() {
        QObject::connect(&server_, &QTcpServer::newConnection, [this]() {
            while (QTcpSocket* socket = server_.nextPendingConnection()) {
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                    request_ += socket->readAll();

                    if (!request_.contains("\r\n\r\n")) {
                        return;
                    }

                    const QByteArray path = request_.split(' ').value(1);
                    request_.clear();
                    const auto found = files_.find(QString::fromLatin1(path));
                    const QByteArray body = found == files_.end() ? QByteArray("not here") : found.value();
                    const QByteArray status = found == files_.end() ? "404 Not Found" : "200 OK";
                    socket->write("HTTP/1.1 " + status + "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        server_.listen(QHostAddress::LocalHost);
    }

    /**
     * @brief Serve bytes at a path.
     * @param path Like "/SHA256SUMS".
     * @param body The bytes.
     */
    void serve(const QString& path, const QByteArray& body) {
        files_.insert(path, body);
    }

    /**
     * @brief The address of a path on this server.
     * @param path Like "/SHA256SUMS".
     * @return Its URL.
     */
    QUrl url(const QString& path) const {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(server_.serverPort()).arg(path));
    }

  private:
    QTcpServer server_;
    QMap<QString, QByteArray> files_;
    QByteArray request_;
};

/**
 * @brief Hash bytes the way SHA256SUMS lists them.
 * @param data The bytes.
 * @return The SHA-256 in lowercase hex.
 */
QByteArray digest(const QByteArray& data) {
    Sha256 hash;
    hash.update(data.constData(), std::size_t(data.size()));
    return QByteArray::fromStdString(hash.hex_digest());
}

/**
 * @brief Read a whole file.
 * @param path The file.
 * @return What's in it, or empty when it's missing.
 */
QByteArray read(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/**
 * @brief Write a whole file, making its folder.
 * @param path The file.
 * @param data What goes in it.
 */
void write(const QString& path, const QByteArray& data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(data);
}

class UpdateInstallerContract : public ::testing::Test {
  protected:
    /**
     * @brief Install a release from the local server and wait for it to end.
     * @param target What gets updated.
     * @param package The release file's bytes.
     * @param sums The SHA256SUMS text to serve.
     * @return How it went.
     */
    UpdateInstaller::Outcome install(const SystemUpdateInstaller::Target& target, const QByteArray& package, const QByteArray& sums) {
        SystemUpdateInstaller installer(target, nullptr);
        const QString name = installer.package_name(QString::fromLatin1(version));
        server.serve(QStringLiteral("/package"), package);
        server.serve(QStringLiteral("/SHA256SUMS"), sums);
        std::optional<UpdateInstaller::Outcome> outcome;
        installer.install({QString::fromLatin1(version), server.url(QStringLiteral("/package")), server.url(QStringLiteral("/SHA256SUMS"))},
                          [this](const UpdateInstaller::Progress& step) {
                              installing = installing || step.installing;
                          },
                          [&outcome](const UpdateInstaller::Outcome& result) {
                              outcome = result;
                          });
        QElapsedTimer timer;
        timer.start();

        while (!outcome && timer.elapsed() < timeout_ms) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, poll_ms);
            QThread::msleep(poll_ms);
        }

        EXPECT_TRUE(outcome.has_value()) << "the install didn't finish";
        return outcome.value_or(UpdateInstaller::Outcome{});
    }

    /**
     * @brief A SHA256SUMS line for a file.
     * @param data Its bytes.
     * @param name Its name.
     * @return The line.
     */
    static QByteArray sum(const QByteArray& data, const QString& name) {
        return digest(data) + "  " + name.toUtf8() + "\n";
    }

    /**
     * @brief Describe an AppImage install in the temporary folder.
     * @return The target.
     */
    SystemUpdateInstaller::Target appimage() const {
        SystemUpdateInstaller::Target target;
        target.kind = UpdatePackage::Kind::AppImage;
        target.path = AppPaths::to_path(directory.filePath(QStringLiteral("app.AppImage")));
        target.staging = AppPaths::to_path(directory.filePath(QStringLiteral(".lom-native-update")));
        return target;
    }

    QTemporaryDir directory;
    FileServer server;
    bool installing = false;
};

}

TEST_F(UpdateInstallerContract, puts_a_checked_appimage_in_place_of_the_old_one) {
    const SystemUpdateInstaller::Target target = appimage();
    write(AppPaths::to_text(target.path), "old version");
    const QByteArray package = "new version";

    const auto outcome = install(target, package, sum("other", "ffmpeg.tar.xz") + sum(package, "lom-native-9.9.9-linux-x86_64.AppImage"));

    EXPECT_TRUE(outcome.ok) << outcome.error.toStdString();
    EXPECT_TRUE(installing);
    EXPECT_EQ(read(AppPaths::to_text(target.path)), package);
#if !defined(Q_OS_WIN)
    EXPECT_TRUE(QFileInfo(AppPaths::to_text(target.path)).isExecutable());
#endif
    EXPECT_FALSE(QFileInfo::exists(AppPaths::to_text(target.staging)));
}

TEST_F(UpdateInstallerContract, keeps_the_old_version_when_the_download_doesnt_match_its_checksum) {
    const SystemUpdateInstaller::Target target = appimage();
    write(AppPaths::to_text(target.path), "old version");

    const auto outcome = install(target, "tampered", sum("new version", "lom-native-9.9.9-linux-x86_64.AppImage"));

    EXPECT_FALSE(outcome.ok);
    EXPECT_EQ(outcome.error, "the download didn't match its checksum, so it wasn't installed");
    EXPECT_FALSE(installing);
    EXPECT_EQ(read(AppPaths::to_text(target.path)), "old version");
    EXPECT_FALSE(QFileInfo::exists(AppPaths::to_text(target.staging)));
}

TEST_F(UpdateInstallerContract, keeps_the_old_version_when_sha256sums_doesnt_list_the_package) {
    const SystemUpdateInstaller::Target target = appimage();
    write(AppPaths::to_text(target.path), "old version");

    const auto outcome = install(target, "new version", sum("other", "ffmpeg.tar.xz"));

    EXPECT_FALSE(outcome.ok);
    EXPECT_EQ(outcome.error, "the release's SHA256SUMS doesn't list lom-native-9.9.9-linux-x86_64.AppImage");
    EXPECT_EQ(read(AppPaths::to_text(target.path)), "old version");
}

TEST_F(UpdateInstallerContract, says_it_cant_update_when_there_is_nothing_to_update) {
    SystemUpdateInstaller installer(std::nullopt, nullptr);
    std::optional<UpdateInstaller::Outcome> outcome;

    installer.install({QString::fromLatin1(version), QUrl(), QUrl()}, nullptr, [&outcome](const UpdateInstaller::Outcome& result) {
        outcome = result;
    });

    EXPECT_TRUE(installer.package_name(QString::fromLatin1(version)).isEmpty());
    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome->ok);
}

#if defined(Q_OS_WIN)
TEST_F(UpdateInstallerContract, unpacks_a_windows_zip_over_the_installed_folder) {
    SystemUpdateInstaller::Target target;
    target.kind = UpdatePackage::Kind::WindowsFolder;
    target.path = AppPaths::to_path(directory.filePath(QStringLiteral("install")));
    target.staging = target.path / ".lom-native-update";
    write(directory.filePath(QStringLiteral("install/Legend of Mana Native.exe")), "old launcher");
    write(directory.filePath(QStringLiteral("install/removed.dll")), "only in the old version");
    ZipWriter zip;
    zip.add("Legend of Mana Native/Legend of Mana Native.exe", "new launcher", std::time(nullptr));
    zip.add("Legend of Mana Native/licenses/a.txt", "new licence", std::time(nullptr));
    const QByteArray package = QByteArray::fromStdString(zip.finish());

    const auto outcome = install(target, package, sum(package, "lom-native-9.9.9-windows-x64.zip"));

    EXPECT_TRUE(outcome.ok) << outcome.error.toStdString();
    EXPECT_EQ(read(directory.filePath(QStringLiteral("install/Legend of Mana Native.exe"))), "new launcher");
    EXPECT_EQ(read(directory.filePath(QStringLiteral("install/licenses/a.txt"))), "new licence");
    EXPECT_EQ(read(directory.filePath(QStringLiteral("install/removed.dll"))), "only in the old version");
}
#endif

#if defined(Q_OS_MACOS)
TEST_F(UpdateInstallerContract, copies_a_mac_app_out_of_its_disk_image) {
    SystemUpdateInstaller::Target target;
    target.kind = UpdatePackage::Kind::MacBundle;
    target.path = AppPaths::to_path(directory.filePath(QStringLiteral("Applications/Legend of Mana Native.app")));
    target.staging = AppPaths::to_path(directory.filePath(QStringLiteral("Applications/.lom-native-update")));
    write(directory.filePath(QStringLiteral("Applications/Legend of Mana Native.app/Contents/Info.plist")), "old");
    write(directory.filePath(QStringLiteral("image/Legend of Mana Native.app/Contents/Info.plist")), "new");
    const QString dmg = directory.filePath(QStringLiteral("package.dmg"));
    ASSERT_EQ(QProcess::execute(QStringLiteral("/usr/bin/hdiutil"), {QStringLiteral("create"), QStringLiteral("-quiet"), QStringLiteral("-srcfolder"),
                                                                     directory.filePath(QStringLiteral("image")), QStringLiteral("-format"),
                                                                     QStringLiteral("UDZO"), dmg}),
              0);
    const QByteArray package = read(dmg);

    const auto outcome = install(target, package, sum(package, "lom-native-9.9.9-macos-arm64.dmg"));

    EXPECT_TRUE(outcome.ok) << outcome.error.toStdString();
    EXPECT_EQ(read(directory.filePath(QStringLiteral("Applications/Legend of Mana Native.app/Contents/Info.plist"))), "new");
}
#endif
