#include "lom/launcher/common/sha1.h"

#include <gtest/gtest.h>

#include <string>

namespace {

using namespace lom::launcher;

struct Vector {
    const char* name;
    std::string input;
    const char* digest;
};

class Sha1Vectors : public ::testing::TestWithParam<Vector> {};

TEST_P(Sha1Vectors, matches_the_published_digest) {
    Sha1 hash;
    hash.update(GetParam().input.data(), GetParam().input.size());
    EXPECT_EQ(hash.hex_digest(), GetParam().digest);
}

INSTANTIATE_TEST_SUITE_P(
    Fips180, Sha1Vectors,
    ::testing::Values(Vector{"empty", "", "da39a3ee5e6b4b0d3255bfef95601890afd80709"}, Vector{"abc", "abc", "a9993e364706816aba3e25717850c26c9cd0d89d"},
                      Vector{"two_blocks", "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", "84983e441c3bd26ebaae4aa1f95129e5e54670f1"},
                      Vector{"million_a", std::string(1000000, 'a'), "34aa973cd4c4daa4f61eeb2bdbad27316534016f"}),
    [](const ::testing::TestParamInfo<Vector>& info) {
        return std::string(info.param.name);
    });

TEST(Sha1, gives_the_same_digest_when_input_arrives_in_pieces) {
    const std::string block(1000, 'a');
    Sha1 hash;

    for (int i = 0; i < 1000; i++) {
        hash.update(block.data(), block.size());
    }

    EXPECT_EQ(hash.hex_digest(), "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
}

}
