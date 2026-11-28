#include "lom/launcher/common/random_provider.h"

#include <mutex>
#include <random>

namespace lom::launcher {

namespace {
class SystemRandomProvider : public RandomProvider {
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

RandomProvider& RandomProvider::system() {
    static SystemRandomProvider source;
    return source;
}

}
