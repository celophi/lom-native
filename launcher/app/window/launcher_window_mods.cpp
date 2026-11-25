#include "window/launcher_window.h"

#include "dialogs/mods_dialog.h"

namespace lom::launcher {

void LauncherWindow::open_mods() {
    const ReleaseCatalog::Release* release = settings_.active_installation ? active_release() : nullptr;
    ModsDialog dialog(ModLibrary(paths_.mods_directory(), paths_.bundled_mods_directory(), release ? release->serial : std::string()), *services_.dialogs,
                      *services_.desktop, this);
    dialog.exec();
}

}
