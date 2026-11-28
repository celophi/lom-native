#include "services/update_installer.h"

#include "lom/launcher/common/files_provider.h"
#include "lom/launcher/common/random_provider.h"
#include "window/app_paths.h"

#include <QCoreApplication>
#include <QThread>

#include <string>
#include <system_error>

namespace lom::launcher {

namespace {

constexpr char staging_name[] = ".lom-native-update";
constexpr char write_check_name[] = ".lom-native-write-check-";
constexpr char windows_program[] = "Legend of Mana Native.exe";
constexpr char windows_folder[] = "Legend of Mana Native";
constexpr char mac_bundle[] = "Legend of Mana Native.app";
constexpr char unpacked_name[] = "unpacked";
constexpr char mount_name[] = "mount";
constexpr int tool_timeout_ms = 10 * 60 * 1000;
constexpr qint64 max_checksums_bytes = 64 * 1024;
constexpr qint64 max_package_bytes = qint64(4) * 1024 * 1024 * 1024;

/**
 * @brief Check that the launcher can make files in a folder.
 * @param folder The folder.
 * @return True when a file could be made there.
 */
bool writable(const std::filesystem::path& folder) {
    const std::filesystem::path probe = folder / (write_check_name + std::to_string(RandomProvider::system().next()));
    bool made = false;

    {
        const auto stream = files_provider().open_write(probe, std::ios::binary | std::ios::trunc);
        made = stream && *stream;
    }

    std::error_code ignored;
    files_provider().remove(probe, ignored);
    return made;
}

/**
 * @brief Run a system tool and wait for it.
 * @param tools Runs it.
 * @param program The tool.
 * @param arguments Its arguments.
 * @return Why it failed, or empty.
 */
QString run_tool(ToolsProvider& tools, const std::filesystem::path& program, const QStringList& arguments) {
    const ToolsProvider::Run run = tools.run(AppPaths::to_text(program), arguments, tool_timeout_ms);
    const QString name = AppPaths::to_text(program.filename());

    if (!run.started) {
        return QStringLiteral("couldn't start %1: %2").arg(name, run.error);
    }

    if (!run.finished) {
        return QStringLiteral("%1 took too long").arg(name);
    }

    if (run.crashed || run.exit_code != 0) {
        return QStringLiteral("%1 failed: %2").arg(name, run.output.trimmed());
    }

    return QString();
}

}

UpdateInstaller::UpdateInstaller(std::optional<Target> target, DownloadProvider& downloads, ToolsProvider& tools, QObject* parent)
    : QObject(parent), target_(std::move(target)), downloads_(downloads), tools_(tools) {
}

UpdateInstaller::~UpdateInstaller() {
    *alive_ = false;

    if (downloading_) {
        downloads_.cancel();
    }

    if (worker_ != nullptr) {
        worker_->wait();
    }
}

std::optional<UpdateInstaller::Target> UpdateInstaller::detect() {
    Target target;
    const QString after = QString::fromLatin1(after_update_argument);
#if defined(Q_OS_WIN)
    const std::filesystem::path program = AppPaths::to_path(QCoreApplication::applicationFilePath());

    if (program.filename() != windows_program || !writable(program.parent_path())) {
        return std::nullopt;
    }

    target.kind = UpdatePackage::Kind::WindowsFolder;
    target.path = program.parent_path();
    target.staging = target.path / staging_name;
    target.tools = AppPaths::to_path(qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"))) / "System32";
    target.program = AppPaths::to_text(program);
    target.arguments = {after};
#elif defined(Q_OS_MACOS)
    constexpr char mac_translocation[] = "/AppTranslocation/";
    const std::filesystem::path bundle = AppPaths::to_path(QCoreApplication::applicationDirPath()).parent_path().parent_path();

    if (bundle.filename() != mac_bundle || AppPaths::to_text(bundle).contains(QLatin1String(mac_translocation)) || !writable(bundle.parent_path())) {
        return std::nullopt;
    }

    target.kind = UpdatePackage::Kind::MacBundle;
    target.path = bundle;
    target.staging = bundle.parent_path() / staging_name;
    target.tools = "/usr/bin";
    target.program = QStringLiteral("/usr/bin/open");
    target.arguments = {QStringLiteral("-n"), AppPaths::to_text(bundle), QStringLiteral("--args"), after};
#else
    const std::filesystem::path appimage = AppPaths::to_path(qEnvironmentVariable("APPIMAGE"));
    std::error_code code;

    if (!qEnvironmentVariable("FLATPAK_ID").isEmpty() || appimage.empty() || !appimage.is_absolute() || !files_provider().is_regular_file(appimage, code) ||
        !writable(appimage.parent_path())) {
        return std::nullopt;
    }

    target.kind = UpdatePackage::Kind::AppImage;
    target.path = appimage;
    target.staging = appimage.parent_path() / staging_name;
    target.program = AppPaths::to_text(appimage);
    target.arguments = {after};
#endif
    return target;
}

QString UpdateInstaller::package_name(const QString& version) const {
    return target_ ? QString::fromStdString(UpdatePackage::asset_name(target_->kind, version.toStdString())) : QString();
}

void UpdateInstaller::install(const Request& request, std::function<void(const Progress&)> progress, std::function<void(const Outcome&)> done) {
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
    files_provider().remove_all(target_->staging, code);
    code.clear();
    files_provider().create_directories(target_->staging, code);

    if (code) {
        finish({false, false, QStringLiteral("couldn't make %1: %2").arg(AppPaths::to_text(target_->staging), QString::fromStdString(code.message()))});
        return;
    }

    const QString name = package_name(request.version);
    package_path_ = target_->staging / AppPaths::to_path(name);
    checksums_.clear();
    downloading_ = true;
    const std::weak_ptr<bool> alive = alive_;
    downloads_.get(
        request.checksums,
        [this, alive](const QByteArray& piece) {
            if (!alive.expired() && *alive.lock()) {
                checksums_.append(piece.left(max_checksums_bytes - checksums_.size()));
            }
        },
        [](qint64, qint64) {},
        [this, alive, name](const DownloadProvider::Done& result) {
            if (alive.expired() || !*alive.lock()) {
                return;
            }

            downloading_ = false;

            if (cancelled_) {
                finish({false, true, QString()});
                return;
            }

            if (!result.ok) {
                finish({false, false, QStringLiteral("couldn't download %1: %2").arg(QString::fromLatin1(UpdatePackage::checksums_name), result.error)});
                return;
            }

            const auto digest = UpdatePackage::checksum(checksums_.toStdString(), name.toStdString());

            if (!digest) {
                finish({false, false, QStringLiteral("the release's %1 doesn't list %2").arg(QString::fromLatin1(UpdatePackage::checksums_name), name)});
                return;
            }

            download_package(QString::fromStdString(*digest));
        });
}

void UpdateInstaller::download_package(const QString& digest) {
    file_ = files_provider().open_write(package_path_, std::ios::binary | std::ios::trunc);

    if (!file_ || !*file_) {
        file_.reset();
        finish({false, false, QStringLiteral("couldn't write %1").arg(AppPaths::to_text(package_path_))});
        return;
    }

    hash_.reset();
    downloading_ = true;
    const std::weak_ptr<bool> alive = alive_;
    downloads_.get(
        request_.package,
        [this, alive](const QByteArray& piece) {
            if (alive.expired() || !*alive.lock() || !write_error_.isEmpty()) {
                return;
            }

            hash_.addData(piece);
            file_->write(piece.constData(), piece.size());

            if (!*file_) {
                write_error_ = QStringLiteral("couldn't write %1").arg(AppPaths::to_text(package_path_));
                downloads_.cancel();
            }
        },
        [this, alive](qint64 received, qint64 total) {
            if (alive.expired() || !*alive.lock() || !write_error_.isEmpty()) {
                return;
            }

            if (received > max_package_bytes) {
                write_error_ = QStringLiteral("the download is too big");
                downloads_.cancel();
                return;
            }

            if (progress_) {
                progress_({received, total, false});
            }
        },
        [this, alive, digest](const DownloadProvider::Done& result) {
            if (alive.expired() || !*alive.lock()) {
                return;
            }

            downloading_ = false;
            file_->flush();

            if (!*file_ && write_error_.isEmpty()) {
                write_error_ = QStringLiteral("couldn't write %1").arg(AppPaths::to_text(package_path_));
            }

            file_.reset();

            if (cancelled_) {
                finish({false, true, QString()});
            } else if (!write_error_.isEmpty()) {
                finish({false, false, write_error_});
            } else if (!result.ok) {
                finish({false, false, QStringLiteral("couldn't download %1: %2").arg(package_name(request_.version), result.error)});
            } else if (QString::fromLatin1(hash_.result().toHex()) != digest) {
                finish({false, false, QStringLiteral("the download didn't match its checksum, so it wasn't installed")});
            } else {
                put_in_place();
            }
        });
}

void UpdateInstaller::put_in_place() {
    if (progress_) {
        progress_({0, 0, true});
    }

    const Target target = *target_;
    const std::filesystem::path package = package_path_;
    ToolsProvider& tools = tools_;
    auto error = std::make_shared<QString>();
    worker_ = QThread::create([target, package, error, &tools]() {
        *error = apply(target, package, tools);
    });
    connect(worker_, &QThread::finished, this, [this, error]() {
        worker_->deleteLater();
        worker_ = nullptr;
        finish({error->isEmpty(), false, *error});
    });
    worker_->start();
}

QString UpdateInstaller::apply(const Target& target, const std::filesystem::path& package, ToolsProvider& tools) {
    std::string error;
    std::error_code code;

    switch (target.kind) {
    case UpdatePackage::Kind::AppImage:
        files_provider().permissions(package,
                                     std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec |
                                         std::filesystem::perms::group_read | std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
                                         std::filesystem::perms::others_exec,
                                     code);

        if (!UpdatePackage::replace_file(target.path, package, &error)) {
            return QString::fromStdString(error);
        }

        return QString();

    case UpdatePackage::Kind::WindowsFolder: {
        const std::filesystem::path unpacked = target.staging / unpacked_name;
        const std::filesystem::path tar = target.tools / "tar.exe";
        files_provider().create_directories(unpacked, code);

        if (!files_provider().exists(tar, code)) {
            return QStringLiteral("Windows' tar.exe is missing, so the download can't be unpacked");
        }

        const QString unpack_error = run_tool(tools, tar, {QStringLiteral("-xf"), AppPaths::to_text(package), QStringLiteral("-C"), AppPaths::to_text(unpacked)});

        if (!unpack_error.isEmpty()) {
            return unpack_error;
        }

        const std::filesystem::path folder = unpacked / windows_folder;

        if (!files_provider().is_regular_file(folder / windows_program, code)) {
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
        const std::filesystem::path hdiutil = target.tools / "hdiutil";
        files_provider().create_directories(mount, code);
        QString tool_error = run_tool(tools, hdiutil,
                                      {QStringLiteral("attach"), AppPaths::to_text(package), QStringLiteral("-nobrowse"), QStringLiteral("-readonly"),
                                       QStringLiteral("-noautoopen"), QStringLiteral("-mountpoint"), AppPaths::to_text(mount)});

        if (tool_error.isEmpty()) {
            tool_error = run_tool(tools, target.tools / "ditto", {AppPaths::to_text(mount / mac_bundle), AppPaths::to_text(staged)});
            run_tool(tools, hdiutil, {QStringLiteral("detach"), AppPaths::to_text(mount), QStringLiteral("-force")});
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

void UpdateInstaller::finish(const Outcome& outcome) {
    busy_ = false;

    if (target_) {
        std::error_code ignored;
        files_provider().remove_all(target_->staging, ignored);
    }

    auto done = std::move(done_);
    done_ = nullptr;
    progress_ = nullptr;

    if (done) {
        done(outcome);
    }
}

void UpdateInstaller::cancel() {
    if (!downloading_) {
        return;
    }

    cancelled_ = true;
    downloads_.cancel();
}

bool UpdateInstaller::restart() {
    return target_ && tools_.start_detached(target_->program, target_->arguments);
}

void UpdateInstaller::clean_up() {
    if (!target_ || busy_) {
        return;
    }

    UpdatePackage::remove_leftovers(target_->path);
    std::error_code ignored;
    files_provider().remove_all(target_->staging, ignored);
}

void UpdateInstaller::wait_for_work() {
    if (worker_ != nullptr) {
        worker_->wait();
    }
}

}
