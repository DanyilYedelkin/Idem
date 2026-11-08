#include <idem/broadphase/brute_force.hpp>

namespace idem {

void BruteForce::find_pairs(std::span<const Aabb> boxes, std::vector<Pair>& out) const {
    out.clear();
    const auto n = static_cast<std::uint32_t>(boxes.size());
    for (std::uint32_t i = 0; i < n; ++i) {
        const Aabb box = boxes[i];
        for (std::uint32_t j = i + 1; j < n; ++j) {
            if (overlaps(box, boxes[j])) {
                out.push_back({i, j});
            }
        }
    }
}

} // namespace idem
