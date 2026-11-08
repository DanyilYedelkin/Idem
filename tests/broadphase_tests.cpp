#include <idem/broadphase/broadphase.hpp>
#include <idem/rng.hpp>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <vector>

using namespace idem;

namespace {

std::vector<Aabb> random_spheres(std::uint64_t seed, std::size_t count, float world, float min_r,
                                 float max_r) {
    Xoshiro256 rng{seed};
    std::vector<Aabb> boxes;
    boxes.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Vec3 c{rng.uniform(-world, world), rng.uniform(-world, world), rng.uniform(-world, world)};
        boxes.push_back(Aabb::around_sphere(c, rng.uniform(min_r, max_r)));
    }
    return boxes;
}

template <BroadPhase T>
std::vector<Pair> sorted_pairs(std::span<const Aabb> boxes) {
    T broad_phase;
    std::vector<Pair> pairs{{123, 456}}; // stale content must be discarded
    broad_phase.find_pairs(boxes, pairs);
    std::ranges::sort(pairs);
    return pairs;
}

void check_pair_invariants(const std::vector<Pair>& sorted, std::size_t box_count) {
    for (const Pair& p : sorted) {
        REQUIRE(p.a < p.b);
        REQUIRE(p.b < box_count);
    }
    REQUIRE(std::ranges::adjacent_find(sorted) == sorted.end());
}

} // namespace

TEMPLATE_TEST_CASE("broad-phase handles trivial inputs", "[broadphase]", BruteForce, UniformGrid, Bvh) {
    CHECK(sorted_pairs<TestType>({}).empty());

    const std::vector<Aabb> one{Aabb::around_sphere({}, 1.0f)};
    CHECK(sorted_pairs<TestType>(one).empty());

    const std::vector<Aabb> two{Aabb::around_sphere({}, 1.0f), Aabb::around_sphere({1.0f, 0.0f, 0.0f}, 1.0f)};
    CHECK(sorted_pairs<TestType>(two) == std::vector<Pair>{{0, 1}});
}

TEMPLATE_TEST_CASE("broad-phase reports touching boxes", "[broadphase]", BruteForce, UniformGrid, Bvh) {
    const std::vector<Aabb> boxes{
        {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{1.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 1.0f}},
        {{2.5f, 0.0f, 0.0f}, {3.5f, 1.0f, 1.0f}},
    };
    CHECK(sorted_pairs<TestType>(boxes) == std::vector<Pair>{{0, 1}});
}

TEMPLATE_TEST_CASE("broad-phase matches brute force on random scenes", "[broadphase]", UniformGrid, Bvh) {
    const auto seed = GENERATE(range<std::uint64_t>(1, 6));
    const auto count = GENERATE(as<std::size_t>{}, 2, 17, 500, 3000);

    SECTION("uniform radii") {
        const auto boxes = random_spheres(seed, count, 20.0f, 0.4f, 0.5f);
        const auto expected = sorted_pairs<BruteForce>(boxes);
        const auto actual = sorted_pairs<TestType>(boxes);
        check_pair_invariants(actual, boxes.size());
        CHECK(actual == expected);
    }

    SECTION("mixed radii") {
        const auto boxes = random_spheres(seed, count, 15.0f, 0.05f, 2.0f);
        CHECK(sorted_pairs<TestType>(boxes) == sorted_pairs<BruteForce>(boxes));
    }

    SECTION("dense cluster") {
        const auto boxes = random_spheres(seed, count, 2.0f, 0.2f, 0.5f);
        CHECK(sorted_pairs<TestType>(boxes) == sorted_pairs<BruteForce>(boxes));
    }
}

TEMPLATE_TEST_CASE("broad-phase survives degenerate distributions", "[broadphase]", UniformGrid, Bvh) {
    SECTION("all centres coincide") {
        // Identical Morton codes for every leaf and a single grid cell.
        const std::vector<Aabb> boxes(64, Aabb::around_sphere({3.0f, 3.0f, 3.0f}, 0.5f));
        const auto pairs = sorted_pairs<TestType>(boxes);
        CHECK(pairs.size() == 64 * 63 / 2);
        CHECK(pairs == sorted_pairs<BruteForce>(boxes));
    }

    SECTION("zero-size boxes") {
        std::vector<Aabb> boxes;
        for (int i = 0; i < 50; ++i) {
            const Vec3 p{static_cast<float>(i % 5), 0.0f, 0.0f};
            boxes.push_back({p, p});
        }
        CHECK(sorted_pairs<TestType>(boxes) == sorted_pairs<BruteForce>(boxes));
    }

    SECTION("points on a line, far apart") {
        std::vector<Aabb> boxes;
        for (int i = 0; i < 100; ++i) {
            boxes.push_back(Aabb::around_sphere({static_cast<float>(i) * 1000.0f, 0.0f, 0.0f}, 0.5f));
        }
        CHECK(sorted_pairs<TestType>(boxes).empty());
    }

    SECTION("one huge box among small ones") {
        auto boxes = random_spheres(11, 400, 30.0f, 0.2f, 0.4f);
        boxes.push_back(Aabb::around_sphere({}, 25.0f));
        CHECK(sorted_pairs<TestType>(boxes) == sorted_pairs<BruteForce>(boxes));
    }
}

TEST_CASE("broad-phase kinds round-trip through their names", "[broadphase]") {
    for (const BroadPhaseKind kind : all_broad_phases) {
        CHECK(parse_broad_phase(to_string(kind)) == kind);
        CHECK(make_broad_phase(kind).index() == static_cast<std::size_t>(kind));
    }
    CHECK_FALSE(parse_broad_phase("octree").has_value());
}
