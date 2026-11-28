#include "lom/launcher/common/random_provider.h"

#include <array>
#include <mutex>
#include <random>

namespace lom::launcher {

namespace {
class SystemRandomProvider : public RandomProvider {
  public:
    /**
     * @brief Seed the generator from the operating system's random device, once.
     */
    SystemRandomProvider() : generator_(seeded()) {
    }

    std::uint32_t next() override {
        const std::lock_guard<std::mutex> lock(mutex_);
        return generator_();
    }

  private:
    /**
     * @brief A generator seeded with several words from the operating system, since some systems' random device
     *        repeats itself when it's drawn from directly.
     * @return The generator.
     */
    static std::mt19937 seeded() {
        std::random_device device;
        std::array<std::uint32_t, 8> words{};

        for (std::uint32_t& word : words) {
            word = device();
        }

        std::seed_seq seed(words.begin(), words.end());
        return std::mt19937(seed);
    }

    std::mutex mutex_;
    std::mt19937 generator_;
};

}

RandomProvider& RandomProvider::system() {
    static SystemRandomProvider source;
    return source;
}

}
