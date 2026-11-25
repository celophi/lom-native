#ifndef LOM_LAUNCHER_MOCK_FILE_DIALOGS_H
#define LOM_LAUNCHER_MOCK_FILE_DIALOGS_H

#include "services/file_dialogs.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class MockFileDialogs : public FileDialogs {
  public:
    MOCK_METHOD(QString, choose_disc_image, (QWidget * parent, const QString& start_directory), (override));
    MOCK_METHOD(QString, choose_game_folder, (QWidget * parent), (override));
    MOCK_METHOD(QString, choose_games_root, (QWidget * parent, const QString& current), (override));
    MOCK_METHOD(QString, choose_save_folder, (QWidget * parent, const QString& current), (override));
    MOCK_METHOD(QString, choose_remaster_folder, (QWidget * parent, const QString& current), (override));
    MOCK_METHOD(QString, choose_mod_folder, (QWidget * parent), (override));
    MOCK_METHOD(QString, choose_memory_card, (QWidget * parent), (override));
};

}

#endif
