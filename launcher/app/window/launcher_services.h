#ifndef LOM_LAUNCHER_LAUNCHER_SERVICES_H
#define LOM_LAUNCHER_LAUNCHER_SERVICES_H

#include "services/desktop_provider.h"
#include "services/download_provider.h"
#include "services/file_dialogs_provider.h"
#include "services/file_lock_provider.h"
#include "lom/launcher/common/clock_provider.h"
#include "lom/launcher/common/random_provider.h"
#include "services/process_runner_provider.h"
#include "services/tools_provider.h"
#include "services/update_feed_provider.h"
#include "services/update_installer.h"

#include <QString>

#include <memory>

namespace lom::launcher {
struct LauncherServices {
    std::unique_ptr<FileDialogsProvider> dialogs;
    std::unique_ptr<ProcessRunnerProvider> processes;
    std::unique_ptr<UpdateFeedProvider> updates;
    std::unique_ptr<DownloadProvider> downloads;
    std::unique_ptr<ToolsProvider> tools;
    std::unique_ptr<UpdateInstaller> installer;
    std::unique_ptr<DesktopProvider> desktop;
    std::unique_ptr<FileLockProvider> locks;
    QString issue_repository;
    const ClockProvider* clock = &ClockProvider::system();
    RandomProvider* random = &RandomProvider::system();

    /**
     * @brief Build the production services.
     * @param update_repository GitHub "owner/name" for update checks and installs; empty disables them.
     */
    static LauncherServices system(const QString& update_repository);
};

}

#endif
