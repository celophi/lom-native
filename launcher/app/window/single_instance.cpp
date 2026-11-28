#include "window/single_instance.h"
#include "lom/launcher/common/files_provider.h"

#include "window/app_paths.h"

#include <QCryptographicHash>
#include <QLocalServer>
#include <QLocalSocket>

#include <system_error>

namespace lom::launcher {

namespace {

constexpr char lock_name[] = "launcher.lock";
constexpr char server_prefix[] = "lom-native-launcher-";
constexpr int connect_timeout_ms = 1000;
constexpr int server_name_hash_chars = 16;

}

SingleInstance::SingleInstance(const std::filesystem::path& data_directory, FileLockProvider& locks, QObject* parent)
    : QObject(parent), locks_(locks), lock_path_(data_directory / lock_name) {
    std::error_code code;
    files_provider().create_directories(data_directory, code);
    const QByteArray hash = QCryptographicHash::hash(AppPaths::to_text(data_directory).toUtf8(), QCryptographicHash::Sha256).toHex();
    server_name_ = QString::fromLatin1(server_prefix) + QString::fromLatin1(hash.left(server_name_hash_chars));
}

SingleInstance::~SingleInstance() {
    if (server_ != nullptr) {
        server_->close();
    }
}

bool SingleInstance::acquire() {
    lock_ = locks_.try_lock(lock_path_);

    if (!lock_) {
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
