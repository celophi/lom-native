#include "test_harness.h"
#include "lom/launcher/common/files_provider.h"

#include <gtest/gtest.h>

#include <random>
#include <system_error>

namespace lom::launcher::test {

namespace {
constexpr std::size_t max_scratch_name_length = 48;

class ScratchCleanup : public ::testing::Environment {
  public:
    /**
     * @brief Remove every scratch directory.
     */
    void TearDown() override {
        TestHarness::instance().remove_all();
    }
};

const ::testing::Environment* const cleanup = ::testing::AddGlobalTestEnvironment(new ScratchCleanup);

}

TestHarness& TestHarness::instance() {
    static TestHarness harness;
    return harness;
}

std::filesystem::path TestHarness::scratch_directory() {
    if (root_.empty()) {
        std::random_device device;
        root_ = std::filesystem::temp_directory_path() / ("lom-launcher-tests-" + std::to_string(device()));
    }

    const ::testing::TestInfo* test = ::testing::UnitTest::GetInstance()->current_test_info();
    const std::string full_name = test == nullptr ? std::string("setup") : test->name();
    std::string name;

    for (const char c : full_name.substr(0, max_scratch_name_length)) {
        name.push_back((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ? c : '-');
    }

    const std::filesystem::path directory = root_ / (name + "-" + std::to_string(counter_++));
    std::error_code code;
    files_provider().create_directories(directory, code);
    return directory;
}

void TestHarness::remove_all() {
    std::error_code code;

    if (!root_.empty()) {
        files_provider().remove_all(root_, code);
    }
}

}
