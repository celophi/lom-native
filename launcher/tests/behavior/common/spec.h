#ifndef LOM_LAUNCHER_SPEC_H
#define LOM_LAUNCHER_SPEC_H

#include <gtest/gtest.h>

#define LOM_SPEC_JOIN_INNER(a, b) a##b
#define LOM_SPEC_JOIN(a, b) LOM_SPEC_JOIN_INNER(a, b)
#define LOM_SPEC_DEFINE(fixture, title, id)                                                                                                                    \
    namespace {                                                                                                                                                \
    struct id : fixture {                                                                                                                                      \
        void TestBody() override;                                                                                                                              \
    };                                                                                                                                                         \
    const ::testing::TestInfo* const LOM_SPEC_JOIN(id, _registered) = ::testing::RegisterTest(#fixture, title, nullptr, nullptr, __FILE__, __LINE__,           \
                                                                                              []() -> fixture* {                                               \
                                                                                                  return new id();                                             \
                                                                                              });                                                              \
    }                                                                                                                                                          \
    void id::TestBody()

#define LOM_SPEC(fixture, title) LOM_SPEC_DEFINE(fixture, title, LOM_SPEC_JOIN(lom_spec_, __LINE__))

#endif
