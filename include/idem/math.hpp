#pragma once

#include <algorithm>
#include <cfloat>
#include <limits>

static_assert(std::numeric_limits<float>::is_iec559, "Idem requires IEEE-754 binary32 floats");

#if defined(FLT_EVAL_METHOD)
static_assert(FLT_EVAL_METHOD == 0, "excess intermediate precision (x87) breaks cross-platform determinism");
#endif

#if defined(__FAST_MATH__) || defined(_M_FP_FAST)
#error "Idem must not be built with fast-math; it allows the compiler to reorder floating-point operations"
#endif

namespace idem {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3& operator+=(Vec3 rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    constexpr Vec3& operator-=(Vec3 rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    constexpr Vec3& operator*=(float s) noexcept {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

    friend constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept { return a += b; }
    friend constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept { return a -= b; }
    friend constexpr Vec3 operator*(Vec3 v, float s) noexcept { return v *= s; }
    friend constexpr Vec3 operator*(float s, Vec3 v) noexcept { return v *= s; }
    friend constexpr Vec3 operator-(Vec3 v) noexcept { return {-v.x, -v.y, -v.z}; }
    friend constexpr bool operator==(Vec3, Vec3) noexcept = default;
};

[[nodiscard]] constexpr float dot(Vec3 a, Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] constexpr float length_squared(Vec3 v) noexcept {
    return dot(v, v);
}

[[nodiscard]] constexpr Vec3 elementwise_min(Vec3 a, Vec3 b) noexcept {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

[[nodiscard]] constexpr Vec3 elementwise_max(Vec3 a, Vec3 b) noexcept {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

struct Aabb {
    Vec3 min;
    Vec3 max;

    [[nodiscard]] static constexpr Aabb empty() noexcept {
        constexpr float inf = std::numeric_limits<float>::infinity();
        return {{inf, inf, inf}, {-inf, -inf, -inf}};
    }

    [[nodiscard]] static constexpr Aabb around_sphere(Vec3 center, float radius) noexcept {
        const Vec3 r{radius, radius, radius};
        return {center - r, center + r};
    }

    constexpr void expand(const Aabb& other) noexcept {
        min = elementwise_min(min, other.min);
        max = elementwise_max(max, other.max);
    }

    [[nodiscard]] constexpr Vec3 center() const noexcept { return (min + max) * 0.5f; }
    [[nodiscard]] constexpr Vec3 extent() const noexcept { return max - min; }

    friend constexpr bool operator==(const Aabb&, const Aabb&) noexcept = default;
};

// Closed intervals: touching boxes overlap. Every broad-phase must agree on this
// or they would report different pair sets for resting contacts.
[[nodiscard]] constexpr bool overlaps(const Aabb& a, const Aabb& b) noexcept {
    return a.min.x <= b.max.x && b.min.x <= a.max.x && //
           a.min.y <= b.max.y && b.min.y <= a.max.y && //
           a.min.z <= b.max.z && b.min.z <= a.max.z;
}

[[nodiscard]] constexpr Aabb merge(Aabb a, const Aabb& b) noexcept {
    a.expand(b);
    return a;
}

} // namespace idem
