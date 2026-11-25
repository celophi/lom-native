#include "lom/launcher/common/sha256.h"

#include <gtest/gtest.h>

#include <string>

namespace {

using namespace lom::launcher;

struct Vector {
    const char* name;
    std::string input;
    const char* digest;
};

class Sha256Vectors : public ::testing::TestWithParam<Vector> {};

TEST_P(Sha256Vectors, matches_the_published_digest) {
    Sha256 hash;
    hash.update(GetParam().input.data(), GetParam().input.size());
    EXPECT_EQ(hash.hex_digest(), GetParam().digest);
}

INSTANTIATE_TEST_SUITE_P(Fips180, Sha256Vectors,
                         ::testing::Values(Vector{"empty", "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
                                           Vector{"abc", "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
                                           Vector{"two_blocks", "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                                                  "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
                                           Vector{"million_a", std::string(1000000, 'a'), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"}),
                         [](const ::testing::TestParamInfo<Vector>& info) {
                             return std::string(info.param.name);
                         });

TEST(Sha256, gives_the_same_digest_when_input_arrives_in_pieces) {
    const std::string block(1000, 'a');
    Sha256 hash;

    for (int i = 0; i < 1000; i++) {
        hash.update(block.data(), block.size());
    }

    EXPECT_EQ(hash.hex_digest(), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

}
