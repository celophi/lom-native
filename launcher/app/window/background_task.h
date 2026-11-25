#ifndef LOM_LAUNCHER_BACKGROUND_TASK_H
#define LOM_LAUNCHER_BACKGROUND_TASK_H

#include "lom/launcher/common/progress.h"

#include <QObject>
#include <QTimer>

#include <atomic>
#include <cstdint>
#include <functional>

class QThread;

namespace lom::launcher {
class BackgroundTask : public QObject {
    Q_OBJECT

  public:
    using Work = std::function<void(const Progress& progress)>;

    /**
     * @brief Create an idle task runner.
     * @param parent Owner.
     */
    explicit BackgroundTask(QObject* parent);

    /**
     * @brief Cancel and wait for any running work.
     */
    ~BackgroundTask() override;

    /**
     * @brief Start work; only one operation runs at a time.
     * @param work Work to run on the worker thread.
     * @return False when work is already running.
     */
    bool start(Work work);

    /**
     * @brief Request cancellation of the running work.
     */
    void cancel();

    /**
     * @brief Report whether work is running.
     * @return True while running.
     */
    bool running() const;

    /**
     * @brief Block until the running work ends; for tests and shutdown.
     */
    void wait();

  signals:
    /**
     * @brief Report sampled progress.
     * @param stage Progress::Stage value.
     * @param done Units completed.
     * @param total Units expected.
     */
    void progressed(int stage, quint64 done, quint64 total);

    /**
     * @brief Report that the work returned, whether completed, failed or cancelled.
     */
    void finished();

  private:
    /**
     * @brief Emit the latest sampled progress if it changed.
     */
    void sample();

    QThread* thread_ = nullptr;
    QTimer timer_;
    std::atomic<bool> cancel_{false};
    std::atomic<int> stage_{0};
    std::atomic<std::uint64_t> done_{0};
    std::atomic<std::uint64_t> total_{0};
    std::uint64_t last_done_ = ~std::uint64_t(0);
    int last_stage_ = -1;
};

}

#endif
