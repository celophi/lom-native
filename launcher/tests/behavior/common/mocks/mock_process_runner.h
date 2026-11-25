#ifndef LOM_LAUNCHER_MOCK_PROCESS_RUNNER_H
#define LOM_LAUNCHER_MOCK_PROCESS_RUNNER_H

#include "services/process_runner.h"

#include <gmock/gmock.h>

namespace lom::launcher::test {
class MockProcessRunner : public ProcessRunner {
  public:
    /**
     * @brief Create the mock.
     */
    MockProcessRunner() : ProcessRunner(nullptr) {
    }

    MOCK_METHOD(void, start, (const Request& request), (override));
    MOCK_METHOD(bool, running, (), (const, override));
};

}

#endif
