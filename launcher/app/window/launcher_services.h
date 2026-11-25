#ifndef LOM_LAUNCHER_LAUNCHER_SERVICES_H
#define LOM_LAUNCHER_LAUNCHER_SERVICES_H

#include "services/desktop.h"
#include "services/file_dialogs.h"
#include "lom/launcher/common/clock.h"
#include "lom/launcher/common/random_source.h"
#include "services/process_runner.h"
#include "services/update_feed.h"

#include <QString>

#include <memory>

namespace lom::launcher {
struct LauncherServices {
    std::unique_ptr<FileDialogs> dialogs;
    std::unique_ptr<ProcessRunner> processes;
    std::unique_ptr<UpdateFeed> updates;
    std::unique_ptr<Desktop> desktop;
    QString issue_repository;
    const Clock* clock = &Clock::system();
    RandomSource* random = &RandomSource::system();

    /**
     * @brief Build the production services.
     * @param update_repository GitHub "owner/name" for update checks; empty disables them.
     */
    static LauncherServices system(const QString& update_repository);
};

}

#endif
