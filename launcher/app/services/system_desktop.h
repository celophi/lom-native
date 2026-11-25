#ifndef LOM_LAUNCHER_SYSTEM_DESKTOP_H
#define LOM_LAUNCHER_SYSTEM_DESKTOP_H

#include "services/desktop.h"

namespace lom::launcher {
class SystemDesktop : public Desktop {
  public:
    /**
     * @brief Show a folder in the platform file manager.
     * @param folder Folder path.
     */
    void open_folder(const QString& folder) override;

    /**
     * @brief Open a web page in the default browser.
     * @param url Page address.
     */
    void open_url(const QUrl& url) override;
};

}

#endif
