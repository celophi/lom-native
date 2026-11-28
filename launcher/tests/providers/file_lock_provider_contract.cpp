#include "services/qt_file_lock_provider.h"

#include <gtest/gtest.h>

#include <QTemporaryDir>

namespace {

using namespace lom::launcher;

class FileLockProviderContract : public ::testing::Test {
  protected:
    QTemporaryDir folder;
    std::filesystem::path lock_path = std::filesystem::u8path(folder.filePath(QStringLiteral("save.lock")).toStdString());
    QtFileLockProvider locks;
};

TEST_F(FileLockProviderContract, takes_a_lock_nobody_holds) {
    const auto lock = locks.try_lock(lock_path);

    EXPECT_NE(lock, nullptr);
}

TEST_F(FileLockProviderContract, wont_take_a_lock_that_is_held) {
    const auto held = locks.try_lock(lock_path);

    const auto second = locks.try_lock(lock_path);

    ASSERT_NE(held, nullptr);
    EXPECT_EQ(second, nullptr);
}

TEST_F(FileLockProviderContract, takes_a_lock_again_once_it_is_let_go) {
    auto held = locks.try_lock(lock_path);
    held.reset();

    const auto again = locks.try_lock(lock_path);

    EXPECT_NE(again, nullptr);
}

}
