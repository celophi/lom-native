#include "window/launcher_window.h"
#include "lom/launcher/common/files_provider.h"

#include "lom/launcher/mods/mod_library.h"
#include "lom/launcher/saves/save_folder.h"
#include "pages/ready_page.h"
#include "services/bug_report.h"
#include "services/game_process.h"
#include "window/background_task.h"
#include "window/ui_text.h"

#include <QDateTime>
#include <QDir>
#include <QSysInfo>
#include <QTimeZone>

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace lom::launcher {

namespace {
constexpr char ending_reported[] = "reported from Help > Report a bug";
constexpr std::size_t kept_game_logs = 10;
constexpr double bytes_per_megabyte = 1024.0 * 1024.0;
constexpr char game_log_prefix[] = "game-";
constexpr char game_log_suffix[] = ".log";
constexpr char game_log_time_format[] = "yyyyMMdd-HHmmss";

/**
 * @brief Remove the oldest game logs beyond the retention limit.
 * @param directory Log directory.
 */
void prune_game_logs(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> files;
    std::vector<std::string> runs;
    std::error_code code;

    for (const FileEntry& entry : files_provider().list(directory, false, code)) {
        const std::string run = BugReport::run_of(entry.path.filename().u8string());

        if (!run.empty()) {
            files.push_back(entry.path);
            runs.push_back(run);
        }
    }

    std::sort(runs.begin(), runs.end());
    runs.erase(std::unique(runs.begin(), runs.end()), runs.end());

    if (runs.size() <= kept_game_logs) {
        return;
    }

    const std::string oldest_kept = runs[runs.size() - kept_game_logs];

    for (const std::filesystem::path& file : files) {
        if (BugReport::run_of(file.filename().u8string()) < oldest_kept) {
            files_provider().remove(file, code);
        }
    }
}
}

void LauncherWindow::launch() {
    const ReleaseCatalog::Release* release = active_release();

    if (release == nullptr || game_->running() || task_->running()) {
        return;
    }

    const std::vector<ModEntry> mods = ModLibrary(paths_.mods_directory(), paths_.bundled_mods_directory(), release->serial).scan();
    const std::vector<std::pair<std::string, std::string>> clashing = ModLibrary::clashing(mods);

    if (!clashing.empty()) {
        auto name = [&mods](const std::string& id) {
            for (const ModEntry& mod : mods) {
                if (mod.id == id) {
                    return QString::fromStdString(mod.name);
                }
            }

            return QString::fromStdString(id);
        };
        QStringList pairs;

        for (const auto& [a, b] : clashing) {
            pairs.append(QString(ui_text::mods_clash_pair).arg(name(a), name(b)));
        }

        show_problem(
            {ui_text::mods_clash_title, QString(ui_text::mods_clash_body).arg(pairs.join(QLatin1Char('\n'))), QString()}, ui_text::mods,
            [this]() {
                show_home();
                open_mods();
            },
            ui_text::back,
            [this]() {
                show_home();
            });
        return;
    }

    const std::filesystem::path root = save_root();
    const bool is_default = root == paths_.default_save_root(release->id);
    SaveFolder::Inspection inspection = SaveFolder::inspect(root, catalog_, release->id);

    if (inspection.status == SaveFolder::Status::Missing && is_default) {
        SaveFolder::initialize(root, nullptr);
        inspection = SaveFolder::inspect(root, catalog_, release->id);
    }

    if (inspection.status == SaveFolder::Status::Missing) {
        show_problem(
            {ui_text::save_missing_title, QString(ui_text::save_missing_body).arg(AppPaths::to_text(root)), QString()}, ui_text::retry,
            [this]() {
                show_home();
                launch();
            },
            ui_text::choose_folder,
            [this]() {
                show_home();
                browse_saves();
            });
        return;
    }

    if (inspection.status != SaveFolder::Status::Ready && inspection.status != SaveFolder::Status::Empty) {
        use_save_root(root);
        return;
    }

    if (inspection.status == SaveFolder::Status::Empty) {
        SaveFolder::initialize(root, nullptr);
    }

    const std::filesystem::path log_file =
        paths_.logs_directory() /
        (game_log_prefix +
         QDateTime::fromSecsSinceEpoch(services_.clock->now_unix_seconds(), QTimeZone::UTC).toString(QString::fromLatin1(game_log_time_format)).toStdString() +
         game_log_suffix);
    QString error;

    const std::filesystem::path sessions = settings_.record_sessions ? paths_.sessions_directory() : std::filesystem::path();
    game_->set_remaster_root(remaster_folder());
    game_->set_mods_directory(paths_.mods_directory());
    game_->set_bundled_mods_directory(paths_.bundled_mods_directory());

    if (!game_->start(paths_.game_executable(release->runtime), settings_.active_installation->data_path, root, paths_.data_directory(), log_file, sessions,
                      &error)) {
        show_problem(
            {ui_text::launch_failed_title, QString(ui_text::launch_failed_body).arg(error, AppPaths::to_text(log_file)), QString()}, ui_text::back,
            [this]() {
                show_home();
            },
            QString(), nullptr);
        return;
    }

    ready_->set_playing(true);
    prune_game_logs(paths_.logs_directory());
}

QString LauncherWindow::bug_report_summary(const std::filesystem::path& run_prefix, const QString& ending) const {
    const QString appimage = qEnvironmentVariable("APPIMAGE");
    const QString flatpak = qEnvironmentVariable("FLATPAK_ID");
    const QString install = !appimage.isEmpty() ? QStringLiteral("AppImage") : !flatpak.isEmpty() ? QStringLiteral("Flatpak") : QStringLiteral("other");
    const ReleaseCatalog::Release* release = active_release();
    QString text = QStringLiteral("Legend of Mana Native bug report\n\n");
    text += QStringLiteral("launcher: %1\n").arg(version_);
    text += QStringLiteral("os: %1 (%2 %3, %4)\n")
                .arg(QSysInfo::prettyProductName(), QSysInfo::kernelType(), QSysInfo::kernelVersion(), QSysInfo::currentCpuArchitecture());
    text += QStringLiteral("install: %1\n").arg(install);
    text += QStringLiteral("game data: %1\n").arg(release ? QString::fromStdString(release->id) : QStringLiteral("none"));
    text += QStringLiteral("game data folder: %1\n")
                .arg(settings_.active_installation ? AppPaths::to_text(settings_.active_installation->data_path) : QStringLiteral("none"));
    text += QStringLiteral("save folder: %1\n").arg(AppPaths::to_text(save_root()));
    text += QStringLiteral("recording sessions: %1\n").arg(settings_.record_sessions ? QStringLiteral("yes") : QStringLiteral("no"));
    const std::filesystem::path remaster = remaster_folder();
    text += QStringLiteral("remaster: %1\n").arg(remaster.empty() ? QStringLiteral("none") : AppPaths::to_text(remaster));
    text += QStringLiteral("run: %1\n").arg(AppPaths::to_text(run_prefix.filename()));
    text += QStringLiteral("how it ended: %1\n").arg(ending);
    return text;
}

void LauncherWindow::report_bug(const std::filesystem::path& run_prefix, const QString& ending) {
    BugReport::Inputs inputs;
    inputs.run_prefix = run_prefix;
    inputs.output_directory = paths_.bug_reports_directory();
    inputs.launcher_log = paths_.launcher_log_file();
    inputs.launcher_settings = paths_.settings_file();
    inputs.save_root = save_root();
    inputs.summary = bug_report_summary(run_prefix, ending);
    inputs.home = QDir::homePath();
    inputs.now = std::time_t(services_.clock->now_unix_seconds());
    QString error;
    const std::optional<BugReport::Result> result = BugReport::build(inputs, &error);

    if (!result) {
        show_problem(
            {ui_text::bug_report_failed_title, QString(ui_text::bug_report_failed_body).arg(error), QString()}, ui_text::back,
            [this]() {
                show_home();
            },
            QString(), nullptr);
        return;
    }

    const QString zip = AppPaths::to_text(result->zip);
    QString body = QString(ui_text::bug_report_saved_body).arg(zip);

    if (result->size > BugReport::github_attachment_limit) {
        body += QString(ui_text::bug_report_too_big).arg(double(result->size) / bytes_per_megabyte, 0, 'f', 1);
    }

    services_.desktop->open_folder(AppPaths::to_text(paths_.bug_reports_directory()));

    if (!services_.issue_repository.isEmpty()) {
        body += QString(ui_text::bug_report_issue);
        services_.desktop->open_url(BugReport::issue_url(services_.issue_repository, version_, *result));
    }

    show_problem(
        {ui_text::bug_report_saved_title, body, QString()}, ui_text::back,
        [this]() {
            show_home();
        },
        ui_text::show_report,
        [this]() {
            services_.desktop->open_folder(AppPaths::to_text(paths_.bug_reports_directory()));
        });
}

void LauncherWindow::report_latest_run() {
    if (game_running()) {
        show_problem(
            {ui_text::busy_title, ui_text::report_while_running, QString()}, ui_text::back,
            [this]() {
                show_home();
            },
            QString(), nullptr);
        return;
    }

    const std::optional<std::filesystem::path> run = BugReport::latest_run(paths_.logs_directory());

    if (!run) {
        show_problem(
            {ui_text::nothing_to_report_title, ui_text::nothing_to_report_body, QString()}, ui_text::back,
            [this]() {
                show_home();
            },
            QString(), nullptr);
        return;
    }

    report_bug(*run, QString::fromLatin1(ending_reported));
}

}
