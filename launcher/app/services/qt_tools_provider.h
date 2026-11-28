#ifndef LOM_LAUNCHER_QT_TOOLS_PROVIDER_H
#define LOM_LAUNCHER_QT_TOOLS_PROVIDER_H

#include "services/tools_provider.h"

namespace lom::launcher {
class QtToolsProvider : public ToolsProvider {
  public:
    Run run(const QString& program, const QStringList& arguments, int timeout_ms) override;

    bool start_detached(const QString& program, const QStringList& arguments) override;
};

}

#endif
