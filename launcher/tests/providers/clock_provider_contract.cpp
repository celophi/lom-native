#include "lom/launcher/common/clock_provider.h"

#include <gtest/gtest.h>

#include <QDateTime>

namespace {

using namespace lom::launcher;

constexpr std::int64_t allowed_seconds = 2;

TEST(ClockProviderContract, gives_the_time_now_in_seconds_since_1970) {
    const std::int64_t before = QDateTime::currentSecsSinceEpoch();

    const std::int64_t now = ClockProvider::system().now_unix_seconds();

    EXPECT_GE(now, before - allowed_seconds);
    EXPECT_LE(now, QDateTime::currentSecsSinceEpoch() + allowed_seconds);
}

TEST(ClockProviderContract, never_goes_backwards_between_two_reads) {
    const std::int64_t first = ClockProvider::system().now_unix_seconds();

    const std::int64_t second = ClockProvider::system().now_unix_seconds();

    EXPECT_GE(second, first);
}

}
