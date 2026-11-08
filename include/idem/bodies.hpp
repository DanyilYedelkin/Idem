#pragma once

#include <idem/math.hpp>

#include <cstddef>
#include <vector>

namespace idem {

// Structure-of-arrays storage. The integrator, the AABB pass and the hasher each
// stream over one or two attributes, so keeping them apart keeps the loops tight.
struct Bodies {
    std::vector<Vec3> position;
    std::vector<Vec3> velocity;
    std::vector<float> radius;
    std::vector<float> inverse_mass;

    [[nodiscard]] std::size_t size() const noexcept { return position.size(); }
    [[nodiscard]] bool empty() const noexcept { return position.empty(); }

    void reserve(std::size_t count) {
        position.reserve(count);
        velocity.reserve(count);
        radius.reserve(count);
        inverse_mass.reserve(count);
    }

    // Uniform density, so mass scales with r^3.
    void add(Vec3 p, Vec3 v, float r) {
        position.push_back(p);
        velocity.push_back(v);
        radius.push_back(r);
        inverse_mass.push_back(1.0f / (r * r * r));
    }

    friend bool operator==(const Bodies&, const Bodies&) = default;
};

} // namespace idem
