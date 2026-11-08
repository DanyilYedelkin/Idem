#include <idem/math.hpp>

#include <catch2/catch_test_macros.hpp>

using idem::Aabb;
using idem::Vec3;

TEST_CASE("Vec3 arithmetic", "[math]") {
    constexpr Vec3 a{1.0f, 2.0f, 3.0f};
    constexpr Vec3 b{4.0f, 5.0f, 6.0f};

    STATIC_CHECK(a + b == Vec3{5.0f, 7.0f, 9.0f});
    STATIC_CHECK(b - a == Vec3{3.0f, 3.0f, 3.0f});
    STATIC_CHECK(a * 2.0f == Vec3{2.0f, 4.0f, 6.0f});
    STATIC_CHECK(-a == Vec3{-1.0f, -2.0f, -3.0f});
    STATIC_CHECK(idem::dot(a, b) == 32.0f);
}

TEST_CASE("AABB overlap uses closed intervals", "[math]") {
    constexpr Aabb unit{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};

    STATIC_CHECK(idem::overlaps(unit, unit));
    STATIC_CHECK(idem::overlaps(unit, Aabb{{1.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 1.0f}}));
    STATIC_CHECK_FALSE(idem::overlaps(unit, Aabb{{1.5f, 0.0f, 0.0f}, {2.0f, 1.0f, 1.0f}}));
    STATIC_CHECK_FALSE(idem::overlaps(unit, Aabb{{0.0f, 0.0f, -2.0f}, {1.0f, 1.0f, -0.5f}}));
}

TEST_CASE("AABB construction and merging", "[math]") {
    constexpr Aabb sphere = Aabb::around_sphere({1.0f, 2.0f, 3.0f}, 0.5f);
    STATIC_CHECK(sphere.min == Vec3{0.5f, 1.5f, 2.5f});
    STATIC_CHECK(sphere.max == Vec3{1.5f, 2.5f, 3.5f});
    STATIC_CHECK(sphere.center() == Vec3{1.0f, 2.0f, 3.0f});

    constexpr Aabb merged = idem::merge(Aabb::empty(), sphere);
    STATIC_CHECK(merged == sphere);

    constexpr Aabb wide = idem::merge(sphere, Aabb{{-1.0f, 0.0f, 0.0f}, {0.0f, 4.0f, 1.0f}});
    STATIC_CHECK(wide.min == Vec3{-1.0f, 0.0f, 0.0f});
    STATIC_CHECK(wide.max == Vec3{1.5f, 4.0f, 3.5f});
}
