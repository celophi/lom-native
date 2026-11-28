#include "services/system_desktop_provider.h"

#include <QDesktopServices>

namespace lom::launcher {

void SystemDesktopProvider::open_folder(const QString& folder) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void SystemDesktopProvider::open_url(const QUrl& url) {
    QDesktopServices::openUrl(url);
}

}
