#pragma once

#include <idem/bodies.hpp>
#include <idem/math.hpp>

#include <cstdint>

namespace idem {

struct SceneConfig {
    std::uint32_t body_count = 10'000;
    std::uint64_t seed = 1;
    float min_radius = 0.25f;
    float max_radius = 0.5f;
    float max_speed = 4.0f;
    // Fraction of the world volume occupied by bodies; the world grows with the
    // body count so that density, and therefore the contact count per body, stays constant.
    float fill_ratio = 0.05f;

    friend bool operator==(const SceneConfig&, const SceneConfig&) = default;
};

struct Scene {
    Aabb bounds;
    Bodies bodies;
};

[[nodiscard]] float world_side_length(const SceneConfig& config);

[[nodiscard]] Scene make_scene(const SceneConfig& config);

} // namespace idem
