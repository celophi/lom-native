#include "window/app_paths.h"
#include "window/launcher_window.h"
#include "window/launcher_services.h"
#include "window/single_instance.h"
#include "services/update_installer.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QRect>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include <functional>
#include <memory>
#include <system_error>

namespace {

using namespace lom::launcher;

constexpr char display_name[] = "Legend of Mana Native";
constexpr qint64 max_log_bytes = 1024 * 1024;
constexpr int after_update_wait_ms = 15000;
constexpr int after_update_retry_ms = 250;

QFile* log_file = nullptr;
QMutex log_mutex;

/**
 * @brief Append Qt diagnostics to the launcher log.
 * @param type Message severity.
 * @param context Source context (unused).
 * @param message Message text.
 */
void write_log(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    Q_UNUSED(context);
    static const char* const levels[] = {"debug", "warning", "critical", "fatal", "info"};
    const QMutexLocker locker(&log_mutex);

    if (log_file != nullptr && log_file->isOpen()) {
        QTextStream out(log_file);
        out << QDateTime::currentDateTime().toString(Qt::ISODate) << ' ' << levels[type] << ' ' << message << '\n';
    }
}

/**
 * @brief Open the launcher log, restarting it when it grows too large.
 * @param paths Launcher paths.
 */
void open_log(const AppPaths& paths) {
    std::error_code code;
    std::filesystem::create_directories(paths.logs_directory(), code);
    log_file = new QFile(AppPaths::to_text(paths.launcher_log_file()));
    const QIODevice::OpenMode mode = log_file->size() > max_log_bytes ? QIODevice::WriteOnly | QIODevice::Truncate : QIODevice::Append;

    if (log_file->open(mode | QIODevice::Text)) {
        qInstallMessageHandler(write_log);
    }
}

}

/**
 * @brief Start the launcher.
 * @param argc Argument count.
 * @param argv Arguments.
 * @return Process exit status.
 */
int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(AppPaths::application_name);
    QApplication::setOrganizationName(QString());
    QApplication::setApplicationDisplayName(display_name);
    QApplication::setApplicationVersion(LOM_APP_VERSION);
    QGuiApplication::setDesktopFileName(QStringLiteral(LOM_APP_ID));

    const AppPaths paths = AppPaths::standard();
    SingleInstance instance(paths.data_directory(), nullptr);
    bool acquired = instance.acquire();

    if (!acquired && QApplication::arguments().contains(QString::fromLatin1(UpdateInstaller::after_update_argument))) {
        for (int waited = 0; !acquired && waited < after_update_wait_ms; waited += after_update_retry_ms) {
            QThread::msleep(after_update_retry_ms);
            acquired = instance.acquire();
        }
    }

    if (!acquired) {
        return 0;
    }

    open_log(paths);
    qInfo("launcher %s starting", LOM_APP_VERSION);

    std::unique_ptr<LauncherWindow> window;
    std::function<void(const QRect*)> open_window = [&](const QRect* geometry) {
        window = std::make_unique<LauncherWindow>(paths, LauncherServices::system(QStringLiteral(LOM_UPDATE_REPOSITORY)), QStringLiteral(LOM_APP_VERSION));
        QObject::connect(window.get(), &LauncherWindow::rebuild_requested, window.get(), [&]() {
            QTimer::singleShot(0, [&]() {
                const QRect geometry = window->geometry();
                std::unique_ptr<LauncherWindow> old = std::move(window);
                open_window(&geometry);
                old.reset();
            });
        });

        if (geometry != nullptr) {
            window->setGeometry(*geometry);
        }

        window->show();
        window->start();
    };
    QObject::connect(&instance, &SingleInstance::activation_requested, [&window]() {
        if (window && !window->game_running()) {
            window->showNormal();
            window->raise();
            window->activateWindow();
        }
    });
    open_window(nullptr);
    return application.exec();
}
