#include <idem/rng.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

using idem::Xoshiro256;

TEST_CASE("xoshiro256** matches the reference implementation", "[rng]") {
    // First outputs of the reference C code for state {1, 2, 3, 4}.
    auto rng = Xoshiro256::from_state({1, 2, 3, 4});
    constexpr std::array<std::uint64_t, 4> expected{11520ull, 0ull, 1509978240ull, 1215971899390074240ull};
    for (const std::uint64_t value : expected) {
        CHECK(rng() == value);
    }
}

TEST_CASE("SplitMix64 seeding is stable", "[rng]") {
    STATIC_CHECK(idem::SplitMix64{0}.next() == 0xE220A8397B1DCDAFull);
}

TEST_CASE("same seed, same sequence", "[rng]") {
    Xoshiro256 a{42};
    Xoshiro256 b{42};
    Xoshiro256 c{43};
    bool any_different = false;
    for (int i = 0; i < 1000; ++i) {
        const auto value = a();
        REQUIRE(value == b());
        any_different = any_different || value != c();
    }
    CHECK(any_different);
}

TEST_CASE("floats lie in the unit interval and ranges are respected", "[rng]") {
    Xoshiro256 rng{7};
    for (int i = 0; i < 100'000; ++i) {
        const float f = rng.next_float();
        REQUIRE(f >= 0.0f);
        REQUIRE(f < 1.0f);

        const float u = rng.uniform(-3.0f, 5.0f);
        REQUIRE(u >= -3.0f);
        REQUIRE(u < 5.0f);

        REQUIRE(rng.below(10) < 10u);
    }
}

TEST_CASE("below() is roughly uniform", "[rng]") {
    Xoshiro256 rng{99};
    constexpr std::uint32_t buckets = 7;
    constexpr int samples = 70'000;
    std::array<int, buckets> counts{};
    for (int i = 0; i < samples; ++i) {
        ++counts[rng.below(buckets)];
    }
    for (const int count : counts) {
        CHECK(count > 9'500);
        CHECK(count < 10'500);
    }
}
