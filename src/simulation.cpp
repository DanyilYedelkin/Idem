#include <idem/simulation.hpp>

#include <idem/state_hash.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace idem {

namespace {

// Fraction of penetration removed per step; a full correction overshoots in stacks.
constexpr float position_correction = 0.8f;

void bounce(float& position, float& velocity, float radius, float lo, float hi, float restitution) noexcept {
    if (position - radius < lo) {
        position = lo + radius;
        if (velocity < 0.0f) {
            velocity = -velocity * restitution;
        }
    } else if (position + radius > hi) {
        position = hi - radius;
        if (velocity > 0.0f) {
            velocity = -velocity * restitution;
        }
    }
}

} // namespace

Simulation::Simulation(Scene scene, const SimulationParams& params, BroadPhaseKind broad_phase)
    : bodies_{std::move(scene.bodies)}, bounds_{scene.bounds}, params_{params}, kind_{broad_phase},
      broad_phase_{make_broad_phase(broad_phase)} {
    boxes_.reserve(bodies_.size());
}

StepStats Simulation::step(std::span<const Command> commands) {
    apply(commands);
    integrate();
    resolve_walls();
    update_boxes();

    std::visit([this](auto& bp) { bp.find_pairs(boxes_, pairs_); }, broad_phase_);

    StepStats stats;
    stats.candidate_pairs = pairs_.size();
    stats.contacts = resolve_contacts();
    ++tick_;
    return stats;
}

std::uint64_t Simulation::state_hash() const noexcept {
    return hash_state(bodies_, tick_);
}

void Simulation::apply(std::span<const Command> commands) {
    for (const Command& cmd : commands) {
        if (cmd.body >= bodies_.size()) {
            throw std::out_of_range{"command targets a body that does not exist"};
        }
        bodies_.velocity[cmd.body] += cmd.impulse * bodies_.inverse_mass[cmd.body];
    }
}

// Semi-implicit Euler: velocity first, then position with the new velocity.
void Simulation::integrate() {
    const Vec3 dv = params_.gravity * params_.dt;
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        bodies_.velocity[i] += dv;
        bodies_.position[i] += bodies_.velocity[i] * params_.dt;
    }
}

void Simulation::resolve_walls() {
    const float e = params_.restitution;
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        Vec3& p = bodies_.position[i];
        Vec3& v = bodies_.velocity[i];
        const float r = bodies_.radius[i];
        bounce(p.x, v.x, r, bounds_.min.x, bounds_.max.x, e);
        bounce(p.y, v.y, r, bounds_.min.y, bounds_.max.y, e);
        bounce(p.z, v.z, r, bounds_.min.z, bounds_.max.z, e);
    }
}

void Simulation::update_boxes() {
    boxes_.resize(bodies_.size());
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        boxes_[i] = Aabb::around_sphere(bodies_.position[i], bodies_.radius[i]);
    }
}

std::size_t Simulation::resolve_contacts() {
    // Contacts are solved sequentially (Gauss-Seidel), so each one sees the
    // corrections made by the ones before it and the order is part of the result.
    // Broad-phases emit pairs in arbitrary order; sorting removes that dependency.
    std::ranges::sort(pairs_);

    auto& p = bodies_.position;
    auto& v = bodies_.velocity;
    const auto& r = bodies_.radius;
    const auto& w = bodies_.inverse_mass;
    const float e = params_.restitution;

    std::size_t contacts = 0;
    for (const auto [a, b] : pairs_) {
        const Vec3 delta = p[b] - p[a];
        const float distance_sq = length_squared(delta);
        const float reach = r[a] + r[b];
        if (distance_sq >= reach * reach) {
            continue;
        }
        ++contacts;

        // sqrt is one of the few library functions IEEE-754 requires to be correctly rounded.
        const float distance = std::sqrt(distance_sq);
        const Vec3 normal = distance > 0.0f ? delta * (1.0f / distance) : Vec3{0.0f, 1.0f, 0.0f};
        const float w_sum = w[a] + w[b];

        const Vec3 correction = normal * ((reach - distance) * position_correction / w_sum);
        p[a] -= correction * w[a];
        p[b] += correction * w[b];

        const float closing_speed = dot(v[b] - v[a], normal);
        if (closing_speed < 0.0f) {
            const Vec3 impulse = normal * (-(1.0f + e) * closing_speed / w_sum);
            v[a] -= impulse * w[a];
            v[b] += impulse * w[b];
        }
    }
    return contacts;
}

} // namespace idem
