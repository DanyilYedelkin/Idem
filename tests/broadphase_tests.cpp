#include <idem/broadphase/brute_force.hpp>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using namespace idem;

namespace {

template <BroadPhase T>
std::vector<Pair> sorted_pairs(std::span<const Aabb> boxes) {
    T broad_phase;
    std::vector<Pair> pairs{{123, 456}}; // stale content must be discarded
    broad_phase.find_pairs(boxes, pairs);
    std::ranges::sort(pairs);
    return pairs;
}

} // namespace

TEMPLATE_TEST_CASE("broad-phase handles trivial inputs", "[broadphase]", BruteForce) {
    CHECK(sorted_pairs<TestType>({}).empty());

    const std::vector<Aabb> one{Aabb::around_sphere({}, 1.0f)};
    CHECK(sorted_pairs<TestType>(one).empty());

    const std::vector<Aabb> two{Aabb::around_sphere({}, 1.0f), Aabb::around_sphere({1.0f, 0.0f, 0.0f}, 1.0f)};
    CHECK(sorted_pairs<TestType>(two) == std::vector<Pair>{{0, 1}});
}

TEMPLATE_TEST_CASE("broad-phase reports touching boxes", "[broadphase]", BruteForce) {
    const std::vector<Aabb> boxes{
        {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{1.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 1.0f}},
        {{2.5f, 0.0f, 0.0f}, {3.5f, 1.0f, 1.0f}},
    };
    CHECK(sorted_pairs<TestType>(boxes) == std::vector<Pair>{{0, 1}});
}
