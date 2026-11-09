#include <idem/replay.hpp>
#include <idem/state_hash.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using namespace idem;

namespace {

std::vector<std::uint64_t> hash_trace(const Recording& rec) {
    std::vector<std::uint64_t> trace{rec.initial_hash};
    for (std::size_t t = 0; t < rec.tick_count(); ++t) {
        trace.push_back(rec.hash_after(t));
    }
    return trace;
}

} // namespace

TEST_CASE("identical runs produce identical traces", "[determinism]") {
    SceneConfig scene;
    scene.body_count = 1'500;
    scene.seed = 5;
    const auto a = record(scene, SimulationParams{}, BroadPhaseKind::uniform_grid, 120);
    const auto b = record(scene, SimulationParams{}, BroadPhaseKind::uniform_grid, 120);
    CHECK(hash_trace(a) == hash_trace(b));
}

TEST_CASE("the broad-phase does not affect the result", "[determinism]") {
    SceneConfig scene;
    scene.body_count = 1'500;
    scene.seed = 9;
    scene.fill_ratio = 0.15f; // dense enough for multi-body contacts every tick

    const auto reference = hash_trace(record(scene, SimulationParams{}, BroadPhaseKind::brute_force, 150));
    CHECK(hash_trace(record(scene, SimulationParams{}, BroadPhaseKind::uniform_grid, 150)) == reference);
    CHECK(hash_trace(record(scene, SimulationParams{}, BroadPhaseKind::bvh, 150)) == reference);
}

TEST_CASE("the state hash sees single-bit differences", "[determinism]") {
    Bodies bodies;
    bodies.add({1.0f, 2.0f, 3.0f}, {}, 0.5f);
    const auto base = hash_state(bodies, 0);

    CHECK(hash_state(bodies, 1) != base);

    Bodies negative_zero = bodies;
    negative_zero.velocity[0].y = -0.0f;
    CHECK(hash_state(negative_zero, 0) != base);

    Bodies nudged = bodies;
    nudged.position[0].z = std::nextafter(3.0f, 4.0f);
    CHECK(hash_state(nudged, 0) != base);
}

// Pinned on GCC, Clang and MSVC. If this changes, either the change was meant to
// alter simulation results (update the constant and say so in the commit), or
// something broke determinism.
TEST_CASE("golden hash", "[determinism][golden]") {
    SceneConfig scene;
    scene.body_count = 2'000;
    scene.seed = 2026;
    const Recording rec = record(scene, SimulationParams{}, BroadPhaseKind::uniform_grid, 240);
    CHECK(rec.hash_after(rec.tick_count() - 1) == 0x4FDB5EA90122CB1Bull);
}
