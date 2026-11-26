#include "window/launcher_services.h"

#include "services/github_update_feed.h"
#include "services/native_file_dialogs.h"
#include "services/qt_process_runner.h"
#include "services/system_desktop.h"
#include "services/system_update_installer.h"

namespace lom::launcher {

LauncherServices LauncherServices::system(const QString& update_repository) {
    LauncherServices services;
    services.dialogs = std::make_unique<NativeFileDialogs>();
    services.processes = std::make_unique<QtProcessRunner>(nullptr);
    services.desktop = std::make_unique<SystemDesktop>();
    services.issue_repository = update_repository;

    if (!update_repository.isEmpty()) {
        services.updates = std::make_unique<GitHubUpdateFeed>(update_repository, nullptr);
        services.installer = std::make_unique<SystemUpdateInstaller>(nullptr);
    }

    return services;
}

}
