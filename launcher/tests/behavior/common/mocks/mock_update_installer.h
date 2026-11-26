#ifndef LOM_LAUNCHER_MOCK_UPDATE_INSTALLER_H
#define LOM_LAUNCHER_MOCK_UPDATE_INSTALLER_H

#include "services/update_installer.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class MockUpdateInstaller : public UpdateInstaller {
  public:
    MOCK_METHOD(QString, package_name, (const QString& version), (const, override));
    MOCK_METHOD(void, install, (const Request& request, std::function<void(const Progress&)> progress, std::function<void(const Outcome&)> done),
                (override));
    MOCK_METHOD(void, cancel, (), (override));
    MOCK_METHOD(bool, restart, (), (override));
    MOCK_METHOD(void, clean_up, (), (override));
};

}

#endif
