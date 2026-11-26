#include "services/system_update_installer.h"

#include "window/app_paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTemporaryFile>
#include <QThread>

#include <memory>
#include <system_error>

namespace lom::launcher {

namespace {

constexpr char staging_name[] = ".lom-native-update";
constexpr char write_check_name[] = ".lom-native-write-check-XXXXXX";
constexpr char windows_program[] = "Legend of Mana Native.exe";
constexpr char windows_folder[] = "Legend of Mana Native";
constexpr char mac_bundle[] = "Legend of Mana Native.app";
constexpr char user_agent[] = "lom-native-launcher";
constexpr char unpacked_name[] = "unpacked";
constexpr char mount_name[] = "mount";
constexpr int transfer_timeout_ms = 30000;
constexpr int tool_timeout_ms = 10 * 60 * 1000;
constexpr qint64 max_checksums_bytes = 64 * 1024;
constexpr qint64 max_package_bytes = qint64(4) * 1024 * 1024 * 1024;

/**
 * @brief Check that the launcher can make files in a folder.
 * @param folder The folder.
 * @return True when a file could be made there.
 */
bool writable(const QString& folder) {
    QTemporaryFile probe(QDir(folder).filePath(QString::fromLatin1(write_check_name)));
    return probe.open();
}

/**
 * @brief Run a system tool and wait for it.
 * @param program The tool.
 * @param arguments Its arguments.
 * @return Why it failed, or empty.
 */
QString run_tool(const QString& program, const QStringList& arguments) {
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    const QString name = QFileInfo(program).fileName();

    if (!process.waitForStarted()) {
        return QStringLiteral("couldn't start %1: %2").arg(name, process.errorString());
    }

    if (!process.waitForFinished(tool_timeout_ms)) {
        process.kill();
        process.waitForFinished();
        return QStringLiteral("%1 took too long").arg(name);
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return QStringLiteral("%1 failed: %2").arg(name, QString::fromLocal8Bit(process.readAll()).trimmed());
    }

    return QString();
}

/**
 * @brief Make a request with the headers every download sends.
 * @param url The address.
 * @return The request.
 */
QNetworkRequest download_request(const QUrl& url) {
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", user_agent);
    request.setTransferTimeout(transfer_timeout_ms);
    return request;
}

}

SystemUpdateInstaller::SystemUpdateInstaller(QObject* parent) : SystemUpdateInstaller(detect(), parent) {
}

SystemUpdateInstaller::SystemUpdateInstaller(std::optional<Target> target, QObject* parent) : QObject(parent), target_(std::move(target)) {
}

SystemUpdateInstaller::~SystemUpdateInstaller() {
    if (reply_ != nullptr) {
        reply_->disconnect(this);
        reply_->abort();
    }

    if (worker_ != nullptr) {
        worker_->wait();
    }
}

std::optional<SystemUpdateInstaller::Target> SystemUpdateInstaller::detect() {
    Target target;
    const QString after = QString::fromLatin1(after_update_argument);
#if defined(Q_OS_WIN)
    const QFileInfo program(QCoreApplication::applicationFilePath());

    if (program.fileName() != QLatin1String(windows_program) || !writable(program.absolutePath())) {
        return std::nullopt;
    }

    target.kind = UpdatePackage::Kind::WindowsFolder;
    target.path = AppPaths::to_path(program.absolutePath());
    target.staging = target.path / staging_name;
    target.program = program.absoluteFilePath();
    target.arguments = {after};
#elif defined(Q_OS_MACOS)
    constexpr char mac_translocation[] = "/AppTranslocation/";
    QDir contents(QCoreApplication::applicationDirPath());
    contents.cdUp();
    contents.cdUp();
    const QFileInfo bundle(contents.absolutePath());

    if (bundle.fileName() != QLatin1String(mac_bundle) || bundle.absoluteFilePath().contains(QLatin1String(mac_translocation)) ||
        !writable(bundle.absolutePath())) {
        return std::nullopt;
    }

    target.kind = UpdatePackage::Kind::MacBundle;
    target.path = AppPaths::to_path(bundle.absoluteFilePath());
    target.staging = AppPaths::to_path(bundle.absolutePath()) / staging_name;
    target.program = QStringLiteral("/usr/bin/open");
    target.arguments = {QStringLiteral("-n"), bundle.absoluteFilePath(), QStringLiteral("--args"), after};
#else
    const QFileInfo appimage(qEnvironmentVariable("APPIMAGE"));

    if (!qEnvironmentVariable("FLATPAK_ID").isEmpty() || qEnvironmentVariable("APPIMAGE").isEmpty() || !appimage.isFile() ||
        !writable(appimage.absolutePath())) {
        return std::nullopt;
    }

    target.kind = UpdatePackage::Kind::AppImage;
    target.path = AppPaths::to_path(appimage.absoluteFilePath());
    target.staging = AppPaths::to_path(appimage.absolutePath()) / staging_name;
    target.program = appimage.absoluteFilePath();
    target.arguments = {after};
#endif
    return target;
}

QString SystemUpdateInstaller::package_name(const QString& version) const {
    return target_ ? QString::fromStdString(UpdatePackage::asset_name(target_->kind, version.toStdString())) : QString();
}

void SystemUpdateInstaller::install(const Request& request, std::function<void(const Progress&)> progress, std::function<void(const Outcome&)> done) {
    if (busy_) {
        return;
    }

    busy_ = true;
    cancelled_ = false;
    write_error_.clear();
    request_ = request;
    progress_ = std::move(progress);
    done_ = std::move(done);

    if (!target_) {
        finish({false, false, QStringLiteral("this copy of the launcher can't update itself")});
        return;
    }

    std::error_code code;
    std::filesystem::remove_all(target_->staging, code);
    std::filesystem::create_directories(target_->staging, code);

    if (code) {
        finish({false, false, QStringLiteral("couldn't make %1: %2").arg(AppPaths::to_text(target_->staging), QString::fromStdString(code.message()))});
        return;
    }

    const QString name = package_name(request.version);
    package_path_ = target_->staging / AppPaths::to_path(name);

    if (network_ == nullptr) {
        network_ = new QNetworkAccessManager(this);
    }

    reply_ = network_->get(download_request(request.checksums));
    connect(reply_, &QNetworkReply::finished, this, [this, name]() {
        QNetworkReply* reply = reply_;
        reply_ = nullptr;
        reply->deleteLater();

        if (cancelled_) {
            finish({false, true, QString()});
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            finish({false, false, QStringLiteral("couldn't download %1: %2").arg(QString::fromLatin1(UpdatePackage::checksums_name), reply->errorString())});
            return;
        }

        const auto digest = UpdatePackage::checksum(reply->read(max_checksums_bytes).toStdString(), name.toStdString());

        if (!digest) {
            finish({false, false, QStringLiteral("the release's %1 doesn't list %2").arg(QString::fromLatin1(UpdatePackage::checksums_name), name)});
            return;
        }

        download_package(QString::fromStdString(*digest));
    });
}

void SystemUpdateInstaller::download_package(const QString& digest) {
    file_ = new QFile(AppPaths::to_text(package_path_), this);

    if (!file_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString error = QStringLiteral("couldn't write %1: %2").arg(file_->fileName(), file_->errorString());
        file_->deleteLater();
        file_ = nullptr;
        finish({false, false, error});
        return;
    }

    hash_.reset();
    reply_ = network_->get(download_request(request_.package));

    const auto save = [this]() {
        const QByteArray data = reply_->readAll();
        hash_.addData(data);

        if (write_error_.isEmpty() && file_->write(data) != data.size()) {
            write_error_ = QStringLiteral("couldn't write %1: %2").arg(file_->fileName(), file_->errorString());
            reply_->abort();
        }
    };

    connect(reply_, &QNetworkReply::readyRead, this, save);
    connect(reply_, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (received > max_package_bytes) {
            write_error_ = QStringLiteral("the download is too big");
            reply_->abort();
            return;
        }

        if (progress_) {
            progress_({received, total, false});
        }
    });
    connect(reply_, &QNetworkReply::finished, this, [this, digest, save]() {
        if (reply_->error() == QNetworkReply::NoError) {
            save();
        }

        QNetworkReply* reply = reply_;
        reply_ = nullptr;
        reply->deleteLater();
        file_->close();
        file_->deleteLater();
        file_ = nullptr;

        if (cancelled_) {
            finish({false, true, QString()});
        } else if (!write_error_.isEmpty()) {
            finish({false, false, write_error_});
        } else if (reply->error() != QNetworkReply::NoError) {
            finish({false, false, QStringLiteral("couldn't download %1: %2").arg(package_name(request_.version), reply->errorString())});
        } else if (QString::fromLatin1(hash_.result().toHex()) != digest) {
            finish({false, false, QStringLiteral("the download didn't match its checksum, so it wasn't installed")});
        } else {
            put_in_place();
        }
    });
}

void SystemUpdateInstaller::put_in_place() {
    if (progress_) {
        progress_({0, 0, true});
    }

    const Target target = *target_;
    const std::filesystem::path package = package_path_;
    auto error = std::make_shared<QString>();
    worker_ = QThread::create([target, package, error]() {
        *error = apply(target, package);
    });
    connect(worker_, &QThread::finished, this, [this, error]() {
        worker_->deleteLater();
        worker_ = nullptr;
        finish({error->isEmpty(), false, *error});
    });
    worker_->start();
}

QString SystemUpdateInstaller::apply(const Target& target, const std::filesystem::path& package) {
    std::string error;
    std::error_code code;

    switch (target.kind) {
    case UpdatePackage::Kind::AppImage:
        QFile::setPermissions(AppPaths::to_text(package), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                                                              QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther);

        if (!UpdatePackage::replace_file(target.path, package, &error)) {
            return QString::fromStdString(error);
        }

        return QString();

    case UpdatePackage::Kind::WindowsFolder: {
        const std::filesystem::path unpacked = target.staging / unpacked_name;
        std::filesystem::create_directories(unpacked, code);
        const QString tar = QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"))).filePath(QStringLiteral("System32/tar.exe"));

        if (!QFileInfo::exists(tar)) {
            return QStringLiteral("Windows' tar.exe is missing, so the download can't be unpacked");
        }

        const QString unpack_error = run_tool(tar, {QStringLiteral("-xf"), AppPaths::to_text(package), QStringLiteral("-C"), AppPaths::to_text(unpacked)});

        if (!unpack_error.isEmpty()) {
            return unpack_error;
        }

        const std::filesystem::path folder = unpacked / windows_folder;

        if (!std::filesystem::is_regular_file(folder / windows_program, code)) {
            return QStringLiteral("the download doesn't have Legend of Mana Native in it");
        }

        if (!UpdatePackage::replace_folder(target.path, folder, &error)) {
            return QString::fromStdString(error);
        }

        return QString();
    }

    case UpdatePackage::Kind::MacBundle: {
        const std::filesystem::path mount = target.staging / mount_name;
        const std::filesystem::path staged = target.staging / mac_bundle;
        std::filesystem::create_directories(mount, code);
        QString tool_error = run_tool(QStringLiteral("/usr/bin/hdiutil"), {QStringLiteral("attach"), AppPaths::to_text(package), QStringLiteral("-nobrowse"),
                                                                           QStringLiteral("-readonly"), QStringLiteral("-noautoopen"), QStringLiteral("-mountpoint"),
                                                                           AppPaths::to_text(mount)});

        if (tool_error.isEmpty()) {
            tool_error = run_tool(QStringLiteral("/usr/bin/ditto"), {AppPaths::to_text(mount / mac_bundle), AppPaths::to_text(staged)});
            run_tool(QStringLiteral("/usr/bin/hdiutil"), {QStringLiteral("detach"), AppPaths::to_text(mount), QStringLiteral("-force")});
        }

        if (!tool_error.isEmpty()) {
            return tool_error;
        }

        if (!UpdatePackage::replace_bundle(target.path, staged, &error)) {
            return QString::fromStdString(error);
        }

        return QString();
    }
    }

    return QStringLiteral("this kind of install can't be updated");
}

void SystemUpdateInstaller::finish(const Outcome& outcome) {
    busy_ = false;

    if (target_) {
        std::error_code ignored;
        std::filesystem::remove_all(target_->staging, ignored);
    }

    auto done = std::move(done_);
    done_ = nullptr;
    progress_ = nullptr;

    if (done) {
        done(outcome);
    }
}

void SystemUpdateInstaller::cancel() {
    if (reply_ == nullptr) {
        return;
    }

    cancelled_ = true;
    reply_->abort();
}

bool SystemUpdateInstaller::restart() {
    return target_ && QProcess::startDetached(target_->program, target_->arguments);
}

void SystemUpdateInstaller::clean_up() {
    if (!target_ || busy_) {
        return;
    }

    UpdatePackage::remove_leftovers(target_->path);
    std::error_code ignored;
    std::filesystem::remove_all(target_->staging, ignored);
}

}
