#ifndef LOM_LAUNCHER_MOCK_FILE_DIALOGS_PROVIDER_H
#define LOM_LAUNCHER_MOCK_FILE_DIALOGS_PROVIDER_H

#include "services/file_dialogs_provider.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class FileDialogsProviderMock : public FileDialogsProvider {
  public:
    MOCK_METHOD(QString, choose_file, (QWidget * parent, const QString& title, const QString& start, const QString& filter), (override));
    MOCK_METHOD(QString, choose_folder, (QWidget * parent, const QString& title, const QString& start), (override));
};

}

#endif
