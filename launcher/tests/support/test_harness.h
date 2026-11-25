#ifndef LOM_LAUNCHER_TEST_HARNESS_H
#define LOM_LAUNCHER_TEST_HARNESS_H

#include <filesystem>

namespace lom::launcher::test {
class TestHarness {
  public:
    /**
     * @brief Access the process-wide scratch space.
     * @return The harness.
     */
    static TestHarness& instance();

    /**
     * @brief Create an empty directory for the current test.
     * @return A fresh directory under the system temporary directory, named after the test.
     */
    std::filesystem::path scratch_directory();

    /**
     * @brief Remove every scratch directory created by this process.
     */
    void remove_all();

  private:
    std::filesystem::path root_;
    int counter_ = 0;
};

}

#endif
