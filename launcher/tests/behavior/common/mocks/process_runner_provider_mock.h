#ifndef LOM_LAUNCHER_MOCK_PROCESS_RUNNER_PROVIDER_H
#define LOM_LAUNCHER_MOCK_PROCESS_RUNNER_PROVIDER_H

#include "services/process_runner_provider.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class ProcessRunnerProviderMock : public ProcessRunnerProvider {
  public:
    /**
     * @brief Create the mock.
     */
    ProcessRunnerProviderMock() : ProcessRunnerProvider(nullptr) {
    }

    MOCK_METHOD(void, start, (const Request& request), (override));
    MOCK_METHOD(bool, running, (), (const, override));
};

}

#endif
