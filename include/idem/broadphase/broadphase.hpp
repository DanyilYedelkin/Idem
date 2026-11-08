#pragma once

#include <idem/broadphase/brute_force.hpp>
#include <idem/broadphase/bvh.hpp>
#include <idem/broadphase/pair.hpp>
#include <idem/broadphase/uniform_grid.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>

namespace idem {

static_assert(BroadPhase<BruteForce>);
static_assert(BroadPhase<UniformGrid>);
static_assert(BroadPhase<Bvh>);

enum class BroadPhaseKind : std::uint8_t { brute_force, uniform_grid, bvh };

inline constexpr std::array all_broad_phases{BroadPhaseKind::brute_force, BroadPhaseKind::uniform_grid,
                                             BroadPhaseKind::bvh};

// Alternatives are declared in BroadPhaseKind order.
using AnyBroadPhase = std::variant<BruteForce, UniformGrid, Bvh>;

[[nodiscard]] inline AnyBroadPhase make_broad_phase(BroadPhaseKind kind) {
    switch (kind) {
    case BroadPhaseKind::brute_force:
        return BruteForce{};
    case BroadPhaseKind::uniform_grid:
        return UniformGrid{};
    case BroadPhaseKind::bvh:
        return Bvh{};
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::string_view to_string(BroadPhaseKind kind) {
    switch (kind) {
    case BroadPhaseKind::brute_force:
        return BruteForce::name;
    case BroadPhaseKind::uniform_grid:
        return UniformGrid::name;
    case BroadPhaseKind::bvh:
        return Bvh::name;
    }
    std::unreachable();
}

[[nodiscard]] inline std::optional<BroadPhaseKind> parse_broad_phase(std::string_view text) {
    for (const BroadPhaseKind kind : all_broad_phases) {
        if (to_string(kind) == text) {
            return kind;
        }
    }
    return std::nullopt;
}

} // namespace idem
