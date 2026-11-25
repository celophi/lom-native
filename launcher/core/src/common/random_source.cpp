#include "lom/launcher/common/random_source.h"

#include <mutex>
#include <random>

namespace lom::launcher {

namespace {
class SystemRandomSource : public RandomSource {
  public:
    std::uint32_t next() override {
        const std::lock_guard<std::mutex> lock(mutex_);
        return device_();
    }

  private:
    std::mutex mutex_;
    std::random_device device_;
};

}

RandomSource& RandomSource::system() {
    static SystemRandomSource source;
    return source;
}

}
