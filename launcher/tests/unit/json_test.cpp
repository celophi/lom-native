#include "lom/launcher/common/json.h"

#include <gtest/gtest.h>

#include <string>

namespace {

using namespace lom::launcher;

TEST(Json, decodes_unicode_escapes_to_utf8) {
    const auto parsed = Json::parse(R"({"a":"x\u00e9\n"})", nullptr);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->find("a")->as_string(), "x\xc3\xa9\n");
}

TEST(Json, reads_arrays_of_mixed_values) {
    const auto parsed = Json::parse(R"([1,-2,3.5,true,null])", nullptr);
    ASSERT_TRUE(parsed.has_value());
    ASSERT_EQ(parsed->as_array().size(), 5u);
    EXPECT_EQ(parsed->as_array()[1].as_int(0), -2);
}

TEST(Json, keeps_integers_beyond_double_precision) {
    const auto parsed = Json::parse(R"({"big":9007199254740993})", nullptr);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->find("big")->as_int(0), 9007199254740993LL);
}

TEST(Json, reads_back_what_it_writes) {
    const auto parsed = Json::parse(R"({"b":[1,-2,3.5,true,null],"a":"x\u00e9\n","big":9007199254740993})", nullptr);
    ASSERT_TRUE(parsed.has_value());
    const auto again = Json::parse(parsed->dump(true), nullptr);
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(again->dump(false), parsed->dump(false));
}

TEST(Json, explains_why_text_was_rejected) {
    std::string error;
    EXPECT_FALSE(Json::parse(R"({"a":1,"a":2})", &error));
    EXPECT_FALSE(error.empty());
}

struct Invalid {
    const char* name;
    std::string text;
};

class JsonRejects : public ::testing::TestWithParam<Invalid> {};

TEST_P(JsonRejects, invalid_text) {
    EXPECT_FALSE(Json::parse(GetParam().text, nullptr));
}

INSTANTIATE_TEST_SUITE_P(Json, JsonRejects,
                         ::testing::Values(Invalid{"duplicate_key", R"({"a":1,"a":2})"}, Invalid{"trailing_comma", "[1,]"},
                                           Invalid{"text_after_the_document", "{} trailing"}, Invalid{"leading_zero", "01"},
                                           Invalid{"lone_surrogate", "\"\\ud800\""},
                                           Invalid{"nesting_too_deep", std::string(100, '[') + std::string(100, ']')}),
                         [](const ::testing::TestParamInfo<Invalid>& info) {
                             return std::string(info.param.name);
                         });

}
