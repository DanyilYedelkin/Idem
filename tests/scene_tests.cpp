#include <idem/scene.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace idem;

namespace {

SceneConfig small_scene(std::uint64_t seed) {
    SceneConfig config;
    config.body_count = 400;
    config.seed = seed;
    return config;
}

bool inside(const Aabb& bounds, Vec3 p, float r) {
    return p.x - r >= bounds.min.x && p.x + r <= bounds.max.x && //
           p.y - r >= bounds.min.y && p.y + r <= bounds.max.y && //
           p.z - r >= bounds.min.z && p.z + r <= bounds.max.z;
}

} // namespace

TEST_CASE("scene generation is deterministic", "[scene]") {
    const Scene a = make_scene(small_scene(3));
    const Scene b = make_scene(small_scene(3));
    CHECK(a.bodies == b.bodies);
    CHECK(a.bounds == b.bounds);
    CHECK_FALSE(make_scene(small_scene(4)).bodies == a.bodies);
}

TEST_CASE("world grows with the body count", "[scene]") {
    SceneConfig config;
    config.body_count = 1'000;
    const float small = world_side_length(config);
    config.body_count = 8'000;
    const float large = world_side_length(config);
    CHECK(large >= 2.0f * small - 1.0f);
    CHECK(large <= 2.0f * small + 1.0f);
}

TEST_CASE("generated bodies fit inside the world", "[scene]") {
    const Scene scene = make_scene(small_scene(8));
    REQUIRE(scene.bodies.size() == 400);
    for (std::size_t i = 0; i < scene.bodies.size(); ++i) {
        REQUIRE(inside(scene.bounds, scene.bodies.position[i], scene.bodies.radius[i]));
    }
}
