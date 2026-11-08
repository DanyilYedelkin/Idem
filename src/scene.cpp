#include <idem/scene.hpp>

#include <idem/rng.hpp>

#include <cmath>

namespace idem {

namespace {

constexpr std::uint64_t integer_cbrt_ceil(std::uint64_t value) noexcept {
    std::uint64_t lo = 0;
    std::uint64_t hi = 2'097'152; // 2^21: its cube exceeds any 63-bit input
    while (lo < hi) {
        const std::uint64_t mid = lo + (hi - lo) / 2;
        if (mid * mid * mid < value) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

static_assert(integer_cbrt_ceil(0) == 0);
static_assert(integer_cbrt_ceil(27) == 3);
static_assert(integer_cbrt_ceil(28) == 4);

} // namespace

// std::cbrt is not required to be correctly rounded and libm implementations
// differ, so the world size is computed with basic arithmetic and an integer root.
float world_side_length(const SceneConfig& config) {
    constexpr double four_thirds_pi = 4.0 / 3.0 * 3.14159265358979323846;
    const auto r = static_cast<double>(config.max_radius);
    const double volume = static_cast<double>(config.body_count) * four_thirds_pi * r * r * r /
                          static_cast<double>(config.fill_ratio);
    const auto side = integer_cbrt_ceil(static_cast<std::uint64_t>(std::ceil(volume)));
    return static_cast<float>(std::max<std::uint64_t>(side, 1));
}

Scene make_scene(const SceneConfig& config) {
    const float half = world_side_length(config) * 0.5f;

    Scene scene;
    scene.bounds = {{-half, -half, -half}, {half, half, half}};
    scene.bodies.reserve(config.body_count);

    Xoshiro256 rng{config.seed};
    for (std::uint32_t i = 0; i < config.body_count; ++i) {
        const float r = rng.uniform(config.min_radius, config.max_radius);
        const float extent = half - r;
        // Braced initialisation is sequenced left to right. Function-call arguments are
        // not, and GCC and MSVC do evaluate them in different orders.
        const Vec3 p{rng.uniform(-extent, extent), rng.uniform(-extent, extent),
                     rng.uniform(-extent, extent)};
        const float s = config.max_speed;
        const Vec3 v{rng.uniform(-s, s), rng.uniform(-s, s), rng.uniform(-s, s)};
        scene.bodies.add(p, v, r);
    }
    return scene;
}

} // namespace idem
