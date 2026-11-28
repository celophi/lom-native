#include "window/launcher_services.h"

#include "services/github_update_feed_provider.h"
#include "services/native_file_dialogs_provider.h"
#include "services/qt_file_lock_provider.h"
#include "services/qt_process_runner_provider.h"
#include "services/system_desktop_provider.h"
#include "services/qt_download_provider.h"
#include "services/qt_tools_provider.h"

namespace lom::launcher {

LauncherServices LauncherServices::system(const QString& update_repository) {
    LauncherServices services;
    services.dialogs = std::make_unique<NativeFileDialogsProvider>();
    services.processes = std::make_unique<QtProcessRunnerProvider>(nullptr);
    services.desktop = std::make_unique<SystemDesktopProvider>();
    services.locks = std::make_unique<QtFileLockProvider>();
    services.issue_repository = update_repository;

    if (!update_repository.isEmpty()) {
        services.updates = std::make_unique<GitHubUpdateFeedProvider>(update_repository, nullptr);
        services.downloads = std::make_unique<QtDownloadProvider>(nullptr);
        services.tools = std::make_unique<QtToolsProvider>();
        services.installer = std::make_unique<UpdateInstaller>(UpdateInstaller::detect(), *services.downloads, *services.tools, nullptr);
    }

    return services;
}

}
