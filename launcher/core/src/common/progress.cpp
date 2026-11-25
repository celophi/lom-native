#include "lom/launcher/common/progress.h"

#include <utility>

namespace lom::launcher {

Progress::Progress() = default;

Progress::Progress(Callback callback, const std::atomic<bool>* cancel) : callback_(std::move(callback)), cancel_(cancel) {
}

void Progress::report(Stage stage, std::uint64_t done, std::uint64_t total) const {
    if (callback_) {
        callback_(stage, done, total);
    }
}

bool Progress::cancelled() const {
    return cancel_ != nullptr && cancel_->load(std::memory_order_relaxed);
}

}
