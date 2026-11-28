#include "services/game_process.h"
#include "lom/launcher/common/files_provider.h"

#include "window/app_paths.h"
#include "services/process_runner_provider.h"
#include "window/ui_text.h"

#include <system_error>

namespace lom::launcher {

GameProcess::GameProcess(ProcessRunnerProvider& runner, FileLockProvider& locks, QObject* parent) : QObject(parent), runner_(runner), locks_(locks) {
    connect(&runner_, &ProcessRunnerProvider::started, this, &GameProcess::started);
    connect(&runner_, &ProcessRunnerProvider::failed, this, [this](const QString& message) {
        release_lock();
        emit failed(message);
    });
    connect(&runner_, &ProcessRunnerProvider::finished, this, [this](int exit_code, bool crashed) {
        release_lock();
        Ending ending = Ending::Quit;

        if (crashed || exit_code != 0) {
            ending = !crashed && exit_code == graphics_unavailable_exit_code ? Ending::GraphicsUnavailable : Ending::Error;
        }

        emit exited(exit_code, ending);
    });
}

GameProcess::~GameProcess() {
    release_lock();
}

bool GameProcess::start(const std::filesystem::path& executable, const std::filesystem::path& data_directory, const std::filesystem::path& save_root,
                        const std::filesystem::path& working_directory, const std::filesystem::path& log_file, const std::filesystem::path& session_directory,
                        QString* error) {
    if (running()) {
        *error = ui_text::save_locked;
        return false;
    }

    std::error_code code;

    if (executable.empty() || !files_provider().is_regular_file(executable, code)) {
        *error = ui_text::runtime_missing;
        return false;
    }

    lock_ = locks_.try_lock(save_root / lock_file_name);

    if (!lock_) {
        *error = ui_text::save_locked;
        return false;
    }

    files_provider().create_directories(log_file.parent_path(), code);
    files_provider().create_directories(working_directory, code);
    log_file_ = log_file;

    ProcessRunnerProvider::Request request;
    request.program = AppPaths::to_text(executable);
    request.arguments = {AppPaths::to_text(data_directory)};
    request.environment.insert(card_root_variable, AppPaths::to_text(save_root));
    request.environment.insert(app_id_variable, QStringLiteral(LOM_APP_ID));
    request.environment.insert(no_error_dialog_variable, QStringLiteral("1"));
    std::filesystem::path report_prefix = log_file;
    request.environment.insert(report_prefix_variable, AppPaths::to_text(report_prefix.replace_extension()));
    request.environment.insert(remaster_root_variable, AppPaths::to_text(remaster_root_));
    request.environment.insert(language_variable, ui_text::language());

    if (!mods_directory_.empty()) {
        request.environment.insert(mods_directory_variable, AppPaths::to_text(mods_directory_));
    }

    if (!bundled_mods_directory_.empty()) {
        request.environment.insert(bundled_mods_directory_variable, AppPaths::to_text(bundled_mods_directory_));
    }

    if (!session_directory.empty()) {
        request.environment.insert(record_variable, QStringLiteral("1"));
        request.environment.insert(session_directory_variable, AppPaths::to_text(session_directory));
    }
    request.working_directory = AppPaths::to_text(working_directory);
    request.log_file = AppPaths::to_text(log_file);
    runner_.start(request);
    return true;
}

void GameProcess::set_remaster_root(const std::filesystem::path& folder) {
    remaster_root_ = folder;
}

void GameProcess::set_mods_directory(const std::filesystem::path& folder) {
    mods_directory_ = folder;
}

void GameProcess::set_bundled_mods_directory(const std::filesystem::path& folder) {
    bundled_mods_directory_ = folder;
}

bool GameProcess::running() const {
    return runner_.running();
}

const std::filesystem::path& GameProcess::log_file() const {
    return log_file_;
}

void GameProcess::release_lock() {
    if (lock_) {
        lock_.reset();
    }
}

}
