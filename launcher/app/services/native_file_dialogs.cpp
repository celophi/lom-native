#include "services/native_file_dialogs.h"

#include "window/ui_text.h"

#include <QDir>
#include <QFileDialog>

namespace lom::launcher {

QString NativeFileDialogs::choose_disc_image(QWidget* parent, const QString& start_directory) {
    return QFileDialog::getOpenFileName(parent, ui_text::image_dialog_title, start_directory, ui_text::image_filter);
}

QString NativeFileDialogs::choose_game_folder(QWidget* parent) {
    return QFileDialog::getExistingDirectory(parent, ui_text::folder_dialog_title, QDir::homePath(), QFileDialog::ShowDirsOnly);
}

QString NativeFileDialogs::choose_games_root(QWidget* parent, const QString& current) {
    return QFileDialog::getExistingDirectory(parent, ui_text::games_root_dialog_title, current, QFileDialog::ShowDirsOnly);
}

QString NativeFileDialogs::choose_save_folder(QWidget* parent, const QString& current) {
    return QFileDialog::getExistingDirectory(parent, ui_text::save_dialog_title, current, QFileDialog::ShowDirsOnly);
}

QString NativeFileDialogs::choose_remaster_folder(QWidget* parent, const QString& current) {
    return QFileDialog::getExistingDirectory(parent, ui_text::remaster_dialog_title, current.isEmpty() ? QDir::homePath() : current, QFileDialog::ShowDirsOnly);
}

QString NativeFileDialogs::choose_mod_folder(QWidget* parent) {
    return QFileDialog::getExistingDirectory(parent, ui_text::mod_dialog_title, QDir::homePath(), QFileDialog::ShowDirsOnly);
}

QString NativeFileDialogs::choose_memory_card(QWidget* parent) {
    return QFileDialog::getOpenFileName(parent, ui_text::card_dialog_title, QDir::homePath(), ui_text::card_filter);
}

}
