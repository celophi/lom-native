#include "lom/launcher/common/random_provider.h"

#include <gtest/gtest.h>

#include <set>

namespace {

using namespace lom::launcher;

constexpr int draws = 1000;

TEST(RandomProviderContract, gives_a_different_number_almost_every_time) {
    std::set<std::uint32_t> seen;

    for (int i = 0; i < draws; ++i) {
        seen.insert(RandomProvider::system().next());
    }

    EXPECT_GE(seen.size(), static_cast<std::size_t>(draws - 1));
}

TEST(RandomProviderContract, uses_the_whole_range_of_32_bits) {
    std::uint32_t any_high = 0, all_low = 0xFFFFFFFFu;

    for (int i = 0; i < draws; ++i) {
        const std::uint32_t value = RandomProvider::system().next();
        any_high |= value;
        all_low &= value;
    }

    EXPECT_EQ(any_high, 0xFFFFFFFFu);
    EXPECT_EQ(all_low, 0u);
}

}
