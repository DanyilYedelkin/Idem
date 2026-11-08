#pragma once

#include <idem/bodies.hpp>
#include <idem/broadphase/broadphase.hpp>
#include <idem/input.hpp>
#include <idem/math.hpp>
#include <idem/scene.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace idem {

struct SimulationParams {
    float dt = 1.0f / 60.0f;
    Vec3 gravity{0.0f, -9.81f, 0.0f};
    float restitution = 0.5f;

    friend bool operator==(const SimulationParams&, const SimulationParams&) = default;
};

struct StepStats {
    std::size_t candidate_pairs = 0;
    std::size_t contacts = 0;
};

// Fixed-step rigid sphere simulation. The state after N steps is a pure function
// of the initial scene, the parameters and the command stream; the choice of
// broad-phase affects performance only.
class Simulation {
public:
    Simulation(Scene scene, const SimulationParams& params, BroadPhaseKind broad_phase);

    StepStats step(std::span<const Command> commands);

    [[nodiscard]] std::uint64_t tick() const noexcept { return tick_; }
    [[nodiscard]] const Bodies& bodies() const noexcept { return bodies_; }
    [[nodiscard]] const Aabb& bounds() const noexcept { return bounds_; }
    [[nodiscard]] const SimulationParams& params() const noexcept { return params_; }
    [[nodiscard]] BroadPhaseKind broad_phase() const noexcept { return kind_; }
    [[nodiscard]] std::uint64_t state_hash() const noexcept;

private:
    void apply(std::span<const Command> commands);
    void integrate();
    void resolve_walls();
    void update_boxes();
    std::size_t resolve_contacts();

    Bodies bodies_;
    Aabb bounds_;
    SimulationParams params_;
    BroadPhaseKind kind_;
    AnyBroadPhase broad_phase_;
    std::vector<Aabb> boxes_;
    std::vector<Pair> pairs_;
    std::uint64_t tick_ = 0;
};

} // namespace idem
