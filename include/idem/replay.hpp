#pragma once

#include <idem/broadphase/broadphase.hpp>
#include <idem/recording.hpp>
#include <idem/scene.hpp>
#include <idem/simulation.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace idem {

struct Divergence {
    std::uint64_t tick = 0; // 0 means the regenerated initial state already differs
    std::uint64_t expected_hash = 0;
    std::uint64_t actual_hash = 0;
};

struct ReplayReport {
    std::uint64_t ticks_verified = 0;
    std::uint64_t final_hash = 0;
    std::optional<Divergence> divergence;

    [[nodiscard]] bool ok() const noexcept { return !divergence.has_value(); }
};

using StepObserver = std::function<void(const Simulation&, const StepStats&)>;

// Runs a fresh simulation driven by a seeded InputScript and records it.
[[nodiscard]] Recording record(const SceneConfig& scene, const SimulationParams& params,
                               BroadPhaseKind broad_phase, std::uint32_t ticks,
                               const StepObserver& observer = {});

// Re-simulates from the recorded initial conditions and inputs only, and stops at
// the first tick whose state hash does not match.
[[nodiscard]] ReplayReport replay(const Recording& recording, BroadPhaseKind broad_phase,
                                  const StepObserver& observer = {});

[[nodiscard]] std::uint64_t input_seed(std::uint64_t scene_seed) noexcept;

} // namespace idem
