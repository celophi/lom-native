#include "services/launcher_dialogs.h"

#include "window/ui_text.h"

#include <QDir>

namespace lom::launcher {

LauncherDialogs::LauncherDialogs(FileDialogsProvider& provider) : provider_(provider) {
}

QString LauncherDialogs::choose_disc_image(QWidget* parent, const QString& start_directory) {
    return provider_.choose_file(parent, ui_text::image_dialog_title, start_directory, ui_text::image_filter);
}

QString LauncherDialogs::choose_game_folder(QWidget* parent) {
    return provider_.choose_folder(parent, ui_text::folder_dialog_title, QDir::homePath());
}

QString LauncherDialogs::choose_games_root(QWidget* parent, const QString& current) {
    return provider_.choose_folder(parent, ui_text::games_root_dialog_title, current);
}

QString LauncherDialogs::choose_save_folder(QWidget* parent, const QString& current) {
    return provider_.choose_folder(parent, ui_text::save_dialog_title, current);
}

QString LauncherDialogs::choose_remaster_folder(QWidget* parent, const QString& current) {
    return provider_.choose_folder(parent, ui_text::remaster_dialog_title, current.isEmpty() ? QDir::homePath() : current);
}

QString LauncherDialogs::choose_mod_folder(QWidget* parent) {
    return provider_.choose_folder(parent, ui_text::mod_dialog_title, QDir::homePath());
}

QString LauncherDialogs::choose_memory_card(QWidget* parent) {
    return provider_.choose_file(parent, ui_text::card_dialog_title, QDir::homePath(), ui_text::card_filter);
}

}
