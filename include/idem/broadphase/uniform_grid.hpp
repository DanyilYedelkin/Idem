#pragma once

#include <idem/broadphase/pair.hpp>

namespace idem {

// Dense uniform grid rebuilt every call with a counting sort, so the cost is
// O(n + cells) and allocations are reused between ticks. The cell size adapts to
// the largest box, which keeps the 3x3x3 neighbourhood search exhaustive but
// degrades towards O(n^2) when sizes vary by orders of magnitude.
class UniformGrid {
public:
    static constexpr std::string_view name = "grid";

    void find_pairs(std::span<const Aabb> boxes, std::vector<Pair>& out);

private:
    std::vector<std::uint32_t> cell_of_;
    std::vector<std::uint32_t> cell_start_;
    std::vector<std::uint32_t> sorted_ids_;
    std::vector<Aabb> sorted_boxes_;
};

} // namespace idem
