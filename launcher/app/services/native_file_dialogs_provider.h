#ifndef LOM_LAUNCHER_NATIVE_FILE_DIALOGS_PROVIDER_H
#define LOM_LAUNCHER_NATIVE_FILE_DIALOGS_PROVIDER_H

#include "services/file_dialogs_provider.h"

namespace lom::launcher {
class NativeFileDialogsProvider : public FileDialogsProvider {
  public:
    QString choose_file(QWidget* parent, const QString& title, const QString& start, const QString& filter) override;

    QString choose_folder(QWidget* parent, const QString& title, const QString& start) override;
};

}

#endif
