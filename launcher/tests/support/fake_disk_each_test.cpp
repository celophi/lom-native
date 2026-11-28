#include "fake_disk_each_test.h"

#include <gtest/gtest.h>

#include <memory>

namespace lom::launcher::test {

namespace {

std::unique_ptr<FilesProviderFake> disk;

class FakeDiskEachTest : public ::testing::EmptyTestEventListener {
  public:
    /**
     * @brief Give the test a fresh fake disk, in use before its fixture is made.
     * @param test The test.
     */
    void OnTestStart(const ::testing::TestInfo& test) override {
        (void)test;
        disk = std::make_unique<FilesProviderFake>();
    }

    /**
     * @brief Put the real disk back after the test.
     * @param test The test.
     */
    void OnTestEnd(const ::testing::TestInfo& test) override {
        (void)test;
        disk.reset();
    }
};

const bool listening = (::testing::UnitTest::GetInstance()->listeners().Append(new FakeDiskEachTest), true);

}

FilesProviderFake& fake_disk() {
    return *disk;
}

}
