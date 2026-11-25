#include "services/system_desktop.h"

#include <QDesktopServices>

namespace lom::launcher {

void SystemDesktop::open_folder(const QString& folder) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void SystemDesktop::open_url(const QUrl& url) {
    QDesktopServices::openUrl(url);
}

}
