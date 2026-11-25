#ifndef LOM_LAUNCHER_DESKTOP_H
#define LOM_LAUNCHER_DESKTOP_H

#include <QString>
#include <QUrl>

namespace lom::launcher {
class Desktop {
  public:
    /**
     * @brief Destroy it.
     */
    virtual ~Desktop() = default;

    /**
     * @brief Open a folder in the system's file manager.
     * @param folder The folder.
     */
    virtual void open_folder(const QString& folder) = 0;

    /**
     * @brief Open a link in the system's browser.
     * @param url The link.
     */
    virtual void open_url(const QUrl& url) = 0;
};

}

#endif
