#pragma once

#include <idem/broadphase/pair.hpp>

namespace idem {

// O(n^2) reference implementation; the other broad-phases are tested against it.
class BruteForce {
public:
    static constexpr std::string_view name = "brute";

    void find_pairs(std::span<const Aabb> boxes, std::vector<Pair>& out) const;
};

} // namespace idem
