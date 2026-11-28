#include "services/native_file_dialogs_provider.h"

#include <QFileDialog>

namespace lom::launcher {

QString NativeFileDialogsProvider::choose_file(QWidget* parent, const QString& title, const QString& start, const QString& filter) {
    return QFileDialog::getOpenFileName(parent, title, start, filter);
}

QString NativeFileDialogsProvider::choose_folder(QWidget* parent, const QString& title, const QString& start) {
    return QFileDialog::getExistingDirectory(parent, title, start, QFileDialog::ShowDirsOnly);
}

}
