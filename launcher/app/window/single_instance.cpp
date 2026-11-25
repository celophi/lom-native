#include "window/single_instance.h"

#include "window/app_paths.h"

#include <QCryptographicHash>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>

#include <system_error>

namespace lom::launcher {

namespace {

constexpr char lock_name[] = "launcher.lock";
constexpr char server_prefix[] = "lom-native-launcher-";
constexpr int connect_timeout_ms = 1000;
constexpr int server_name_hash_chars = 16;

}

SingleInstance::SingleInstance(const std::filesystem::path& data_directory, QObject* parent) : QObject(parent) {
    std::error_code code;
    std::filesystem::create_directories(data_directory, code);
    lock_ = std::make_unique<QLockFile>(AppPaths::to_text(data_directory / lock_name));
    const QByteArray hash = QCryptographicHash::hash(AppPaths::to_text(data_directory).toUtf8(), QCryptographicHash::Sha256).toHex();
    server_name_ = QString::fromLatin1(server_prefix) + QString::fromLatin1(hash.left(server_name_hash_chars));
}

SingleInstance::~SingleInstance() {
    if (server_ != nullptr) {
        server_->close();
    }
}

bool SingleInstance::acquire() {
    if (!lock_->tryLock(0)) {
        QLocalSocket socket;
        socket.connectToServer(server_name_);

        if (socket.waitForConnected(connect_timeout_ms)) {
            socket.write("show");
            socket.waitForBytesWritten(connect_timeout_ms);
        }

        return false;
    }

    QLocalServer::removeServer(server_name_);
    server_ = new QLocalServer(this);
    server_->setSocketOptions(QLocalServer::UserAccessOption);
    connect(server_, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket* socket = server_->nextPendingConnection()) {
            socket->deleteLater();
            emit activation_requested();
        }
    });
    server_->listen(server_name_);
    return true;
}

}
