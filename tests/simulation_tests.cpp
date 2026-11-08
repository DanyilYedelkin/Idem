#include <idem/scene.hpp>
#include <idem/simulation.hpp>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace idem;

namespace {

SceneConfig small_scene(std::uint64_t seed = 3) {
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

TEST_CASE("bodies start and stay inside the world", "[simulation]") {
    const SceneConfig config = small_scene();
    Simulation sim{make_scene(config), SimulationParams{}, BroadPhaseKind::uniform_grid};

    const Bodies& b = sim.bodies();
    for (std::size_t i = 0; i < b.size(); ++i) {
        REQUIRE(inside(sim.bounds(), b.position[i], b.radius[i]));
    }

    // Walls are resolved before contacts, so the contact solver may leave a body
    // slightly outside until the next step.
    const float slack = 2.0f * config.max_radius;
    for (int t = 0; t < 300; ++t) {
        sim.step({});
        for (std::size_t i = 0; i < b.size(); ++i) {
            REQUIRE(inside(sim.bounds(), b.position[i], b.radius[i] - slack));
        }
    }
    CHECK(sim.tick() == 300);
}

TEST_CASE("free fall follows semi-implicit Euler", "[simulation]") {
    Scene scene;
    scene.bounds = {{-100.0f, -100.0f, -100.0f}, {100.0f, 100.0f, 100.0f}};
    scene.bodies.add({0.0f, 50.0f, 0.0f}, {}, 0.5f);

    SimulationParams params;
    params.dt = 0.5f;
    params.gravity = {0.0f, -2.0f, 0.0f};
    Simulation sim{std::move(scene), params, BroadPhaseKind::brute_force};

    sim.step({});
    CHECK(sim.bodies().velocity[0] == Vec3{0.0f, -1.0f, 0.0f});
    CHECK(sim.bodies().position[0] == Vec3{0.0f, 49.5f, 0.0f});
    sim.step({});
    CHECK(sim.bodies().velocity[0] == Vec3{0.0f, -2.0f, 0.0f});
    CHECK(sim.bodies().position[0] == Vec3{0.0f, 48.5f, 0.0f});
}

TEST_CASE("head-on collision separates and reverses bodies", "[simulation]") {
    Scene scene;
    scene.bounds = {{-10.0f, -10.0f, -10.0f}, {10.0f, 10.0f, 10.0f}};
    scene.bodies.add({-0.45f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 0.5f);
    scene.bodies.add({0.45f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, 0.5f);

    SimulationParams params;
    params.gravity = {};
    params.restitution = 1.0f;
    Simulation sim{std::move(scene), params, BroadPhaseKind::brute_force};

    const StepStats stats = sim.step({});
    CHECK(stats.candidate_pairs == 1);
    CHECK(stats.contacts == 1);

    const Bodies& b = sim.bodies();
    CHECK(b.velocity[0].x < 0.0f);
    CHECK(b.velocity[1].x > 0.0f);
    CHECK(b.velocity[0].x == -b.velocity[1].x);
    CHECK(b.position[0].x < b.position[1].x);
}

TEST_CASE("commands apply impulses scaled by inverse mass", "[simulation]") {
    Scene scene;
    scene.bounds = {{-10.0f, -10.0f, -10.0f}, {10.0f, 10.0f, 10.0f}};
    scene.bodies.add({}, {}, 0.5f);

    SimulationParams params;
    params.gravity = {};
    Simulation sim{std::move(scene), params, BroadPhaseKind::brute_force};

    const Command push{0, {0.125f, 0.0f, 0.0f}};
    sim.step(std::span{&push, 1});
    CHECK(sim.bodies().velocity[0] == Vec3{1.0f, 0.0f, 0.0f});

    const Command invalid{1, {}};
    CHECK_THROWS_AS(sim.step(std::span{&invalid, 1}), std::out_of_range);
}
