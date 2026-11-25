#ifndef LOM_LAUNCHER_QT_PROCESS_RUNNER_H
#define LOM_LAUNCHER_QT_PROCESS_RUNNER_H

#include "services/process_runner.h"

#include <QProcess>

namespace lom::launcher {
class QtProcessRunner : public ProcessRunner {
    Q_OBJECT

  public:
    /**
     * @brief Create a runner.
     * @param parent Owner.
     */
    explicit QtProcessRunner(QObject* parent);

    /**
     * @brief Wait for a running process to end on its own rather than terminating it.
     */
    ~QtProcessRunner() override;

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
