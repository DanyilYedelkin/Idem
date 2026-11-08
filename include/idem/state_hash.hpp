#pragma once

#include <idem/bodies.hpp>

#include <bit>
#include <cstdint>
#include <span>

namespace idem {

// Fast, non-cryptographic. It only has to make any divergence in the bit
// pattern of the state visible, so floats are hashed as raw bits: 0.0f and
// -0.0f compare equal but are deliberately different states here.
class StateHasher {
public:
    constexpr void add(std::uint64_t value) noexcept {
        state_ = (std::rotl(state_, 23) ^ value) * multiplier;
    }

    constexpr void add(float value) noexcept { add(std::uint64_t{std::bit_cast<std::uint32_t>(value)}); }

    constexpr void add(Vec3 v) noexcept {
        add(v.x);
        add(v.y);
        add(v.z);
    }

    template <class T>
    constexpr void add(std::span<const T> values) noexcept {
        add(std::uint64_t{values.size()});
        for (const T& v : values) {
            add(v);
        }
    }

    [[nodiscard]] constexpr std::uint64_t digest() const noexcept {
        std::uint64_t h = state_;
        h ^= h >> 33;
        h *= 0xFF51AFD7ED558CCDull;
        h ^= h >> 33;
        h *= 0xC4CEB9FE1A85EC53ull;
        h ^= h >> 33;
        return h;
    }

private:
    static constexpr std::uint64_t multiplier = 0x9E3779B97F4A7C15ull;
    std::uint64_t state_ = 0x243F6A8885A308D3ull;
};

[[nodiscard]] inline std::uint64_t hash_state(const Bodies& bodies, std::uint64_t tick) noexcept {
    StateHasher hasher;
    hasher.add(tick);
    hasher.add(std::span<const Vec3>{bodies.position});
    hasher.add(std::span<const Vec3>{bodies.velocity});
    return hasher.digest();
}

} // namespace idem
