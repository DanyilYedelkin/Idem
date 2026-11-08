#pragma once

#include <idem/broadphase/pair.hpp>

namespace idem {

// Linear BVH (Karras 2012): boxes are sorted along a 30-bit Morton curve with a
// radix sort and the hierarchy is emitted in a single pass over adjacent keys.
// Rebuilt from scratch each call; with no persistent state there is nothing to refit.
class Bvh {
public:
    static constexpr std::string_view name = "bvh";

    void find_pairs(std::span<const Aabb> boxes, std::vector<Pair>& out);

private:
    struct Node {
        Aabb box;
        std::uint32_t left = 0;
        std::uint32_t right = 0;
        std::uint32_t last = 0; // highest leaf index below this node
    };

    void sort_by_morton_code(std::span<const Aabb> boxes);
    void build_hierarchy();
    void fit_bounds();
    void collect_pairs(std::vector<Pair>& out) const;

    std::vector<std::uint64_t> keys_;
    std::vector<std::uint64_t> scratch_;
    std::vector<std::uint32_t> leaf_ids_;
    std::vector<Aabb> leaf_boxes_;
    std::vector<Node> nodes_;
    std::vector<std::uint32_t> parents_;
    std::vector<std::uint8_t> visits_;
};

} // namespace idem
