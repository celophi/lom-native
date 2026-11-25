#include "lom/launcher/common/semantic_version.h"

#include <gtest/gtest.h>

#include <string>

namespace {

using namespace lom::launcher;

struct Order {
    const char* name;
    const char* left;
    const char* right;
    int sign;
};

class SemanticVersionOrder : public ::testing::TestWithParam<Order> {};

TEST_P(SemanticVersionOrder, compares_by_semantic_version_rules) {
    const auto left = SemanticVersion::parse(GetParam().left);
    const auto right = SemanticVersion::parse(GetParam().right);
    ASSERT_TRUE(left);
    ASSERT_TRUE(right);
    const int compared = left->compare(*right);
    EXPECT_EQ((compared > 0) - (compared < 0), GetParam().sign);
}

INSTANTIATE_TEST_SUITE_P(SemanticVersion, SemanticVersionOrder,
                         ::testing::Values(Order{"numbers_compare_numerically", "v0.10.0", "0.9.9", 1}, Order{"equal_versions", "1.0.0", "1.0.0", 0},
                                           Order{"prerelease_before_release", "1.0.0-dev", "1.0.0", -1},
                                           Order{"numeric_prerelease_parts_compare_numerically", "1.0.0-alpha.2", "1.0.0-alpha.10", -1},
                                           Order{"build_metadata_is_ignored", "1.0.0+build.5", "1.0.0", 0}),
                         [](const ::testing::TestParamInfo<Order>& info) {
                             return std::string(info.param.name);
                         });

struct Invalid {
    const char* name;
    const char* text;
};

class SemanticVersionRejects : public ::testing::TestWithParam<Invalid> {};

TEST_P(SemanticVersionRejects, invalid_text) {
    EXPECT_FALSE(SemanticVersion::parse(GetParam().text));
}

INSTANTIATE_TEST_SUITE_P(SemanticVersion, SemanticVersionRejects,
                         ::testing::Values(Invalid{"missing_patch", "1.0"}, Invalid{"leading_zero", "1.02.0"}, Invalid{"not_a_version", "latest"}),
                         [](const ::testing::TestParamInfo<Invalid>& info) {
                             return std::string(info.param.name);
                         });

TEST(SemanticVersion, a_suffix_marks_a_prerelease) {
    const auto version = SemanticVersion::parse("0.4.0-dev");
    ASSERT_TRUE(version);
    EXPECT_TRUE(version->is_prerelease());
}

}
