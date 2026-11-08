#pragma once

#include <idem/math.hpp>

#include <compare>
#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace idem {

struct Pair {
    std::uint32_t a = 0;
    std::uint32_t b = 0;

    friend constexpr auto operator<=>(const Pair&, const Pair&) noexcept = default;
};

[[nodiscard]] constexpr Pair ordered_pair(std::uint32_t i, std::uint32_t j) noexcept {
    return i < j ? Pair{i, j} : Pair{j, i};
}

// A broad-phase reports every pair of overlapping boxes exactly once, as
// (lower index, higher index), in an unspecified order. `out` is overwritten.
template <class T>
concept BroadPhase =
    std::default_initializable<T> && requires(T bp, std::span<const Aabb> boxes, std::vector<Pair>& out) {
        { T::name } -> std::convertible_to<std::string_view>;
        { bp.find_pairs(boxes, out) } -> std::same_as<void>;
    };

} // namespace idem
