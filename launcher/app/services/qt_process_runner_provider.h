#ifndef LOM_LAUNCHER_QT_PROCESS_RUNNER_PROVIDER_H
#define LOM_LAUNCHER_QT_PROCESS_RUNNER_PROVIDER_H

#include "services/process_runner_provider.h"

#include <QProcess>

namespace lom::launcher {
class QtProcessRunnerProvider : public ProcessRunnerProvider {
    Q_OBJECT

  public:
    /**
     * @brief Create a runner.
     * @param parent Owner.
     */
    explicit QtProcessRunnerProvider(QObject* parent);

    /**
     * @brief Wait for a running process to end on its own rather than terminating it.
     */
    ~QtProcessRunnerProvider() override;

    /**
     * @brief Start a process.
     * @param request What to start.
     */
    void start(const Request& request) override;

    /**
     * @brief Report whether a process is starting or running.
     * @return True while it exists.
     */
    bool running() const override;

  private:
    QProcess process_;
};

}

#endif
