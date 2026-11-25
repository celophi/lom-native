#ifndef LOM_LAUNCHER_PROGRESS_H
#define LOM_LAUNCHER_PROGRESS_H

#include <atomic>
#include <cstdint>
#include <functional>

namespace lom::launcher {
class Progress {
  public:
    enum class Stage { CheckingImage, Extracting, VerifyingFiles, CheckingFolder, ImportingSaves };

    using Callback = std::function<void(Stage stage, std::uint64_t done, std::uint64_t total)>;

    /**
     * @brief Create progress that reports nothing and is never cancelled.
     */
    Progress();

    /**
     * @brief Create progress with a reporting callback and cancellation flag.
     * @param callback Called from the worker thread; may be empty.
     * @param cancel Flag set by the frontend to request cancellation; may be null.
     */
    Progress(Callback callback, const std::atomic<bool>* cancel);

    /**
     * @brief Report work completed in a stage.
     * @param stage Current stage.
     * @param done Units completed.
     * @param total Units expected in this stage.
     */
    void report(Stage stage, std::uint64_t done, std::uint64_t total) const;

    /**
     * @brief Check whether cancellation was requested.
     * @return True when the worker should stop and clean up.
     */
    bool cancelled() const;

  private:
    Callback callback_;
    const std::atomic<bool>* cancel_ = nullptr;
};

}

#endif
