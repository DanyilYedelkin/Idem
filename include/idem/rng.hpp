#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <limits>

namespace idem {

// <random> engines are fully specified, but its distributions are not:
// std::uniform_real_distribution yields different values on libstdc++ and the
// MSVC STL for the same engine state. Everything here is defined bit-for-bit.

class SplitMix64 {
public:
    explicit constexpr SplitMix64(std::uint64_t seed) noexcept : state_{seed} {}

    constexpr std::uint64_t next() noexcept {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

private:
    std::uint64_t state_;
};

// xoshiro256** by Blackman and Vigna.
class Xoshiro256 {
public:
    using result_type = std::uint64_t;

    explicit constexpr Xoshiro256(std::uint64_t seed) noexcept {
        SplitMix64 seeder{seed};
        for (auto& word : state_) {
            word = seeder.next();
        }
    }

    [[nodiscard]] static constexpr Xoshiro256 from_state(const std::array<std::uint64_t, 4>& state) noexcept {
        Xoshiro256 rng{0};
        rng.state_ = state;
        return rng;
    }

    static constexpr result_type min() noexcept { return 0; }
    static constexpr result_type max() noexcept { return std::numeric_limits<result_type>::max(); }

    constexpr result_type operator()() noexcept {
        auto& s = state_;
        const std::uint64_t result = std::rotl(s[1] * 5, 7) * 9;
        const std::uint64_t t = s[1] << 17;
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = std::rotl(s[3], 45);
        return result;
    }

    constexpr std::uint32_t next_u32() noexcept { return static_cast<std::uint32_t>((*this)() >> 32); }

    // Uniform in [0, 1): the top 24 bits fill the float mantissa exactly.
    constexpr float next_float() noexcept { return static_cast<float>((*this)() >> 40) * 0x1.0p-24f; }

    constexpr float uniform(float lo, float hi) noexcept { return lo + (hi - lo) * next_float(); }

    // Uniform integer in [0, bound) without modulo bias (Lemire, 2019).
    constexpr std::uint32_t below(std::uint32_t bound) noexcept {
        std::uint64_t m = std::uint64_t{next_u32()} * bound;
        auto low = static_cast<std::uint32_t>(m);
        if (low < bound) {
            const std::uint32_t threshold = (0u - bound) % bound;
            while (low < threshold) {
                m = std::uint64_t{next_u32()} * bound;
                low = static_cast<std::uint32_t>(m);
            }
        }
        return static_cast<std::uint32_t>(m >> 32);
    }

private:
    std::array<std::uint64_t, 4> state_{};
};

} // namespace idem
