#ifndef LOM_LAUNCHER_TOOLS_PROVIDER_MOCK_H
#define LOM_LAUNCHER_TOOLS_PROVIDER_MOCK_H

#include "services/tools_provider.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class ToolsProviderMock : public ToolsProvider {
  public:
    MOCK_METHOD(Run, run, (const QString& program, const QStringList& arguments, int timeout_ms), (override));
    MOCK_METHOD(bool, start_detached, (const QString& program, const QStringList& arguments), (override));
};

}

#endif
