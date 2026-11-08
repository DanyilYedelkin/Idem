#include <idem/broadphase/bvh.hpp>

#include <array>
#include <bit>
#include <cassert>
#include <utility>

namespace idem {

namespace {

constexpr std::uint32_t leaf_flag = 0x8000'0000u;
constexpr std::uint32_t morton_axis_max = 1023;

constexpr bool is_leaf(std::uint32_t child) noexcept {
    return (child & leaf_flag) != 0;
}
constexpr std::uint32_t leaf_index(std::uint32_t child) noexcept {
    return child & ~leaf_flag;
}

constexpr std::uint32_t spread_bits(std::uint32_t v) noexcept {
    v &= 0x3ffu;
    v = (v | (v << 16)) & 0x030000ffu;
    v = (v | (v << 8)) & 0x0300f00fu;
    v = (v | (v << 4)) & 0x030c30c3u;
    v = (v | (v << 2)) & 0x09249249u;
    return v;
}

static_assert(spread_bits(0b111) == 0b1001001);

constexpr std::uint32_t morton_code(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
    return spread_bits(x) | (spread_bits(y) << 1) | (spread_bits(z) << 2);
}

} // namespace

void Bvh::find_pairs(std::span<const Aabb> boxes, std::vector<Pair>& out) {
    out.clear();
    if (boxes.size() < 2) {
        return;
    }
    sort_by_morton_code(boxes);
    build_hierarchy();
    fit_bounds();
    collect_pairs(out);
}

void Bvh::sort_by_morton_code(std::span<const Aabb> boxes) {
    const std::size_t n = boxes.size();

    Aabb centroids = Aabb::empty();
    for (const Aabb& box : boxes) {
        const Vec3 c = box.center();
        centroids.expand({c, c});
    }
    const Vec3 extent = centroids.extent();
    auto scale_for = [](float length) {
        return length > 0.0f ? static_cast<float>(morton_axis_max) / length : 0.0f;
    };
    const Vec3 scale{scale_for(extent.x), scale_for(extent.y), scale_for(extent.z)};
    auto quantize = [](float v, float lo, float s) {
        return static_cast<std::uint32_t>(
            std::clamp((v - lo) * s, 0.0f, static_cast<float>(morton_axis_max)));
    };

    // The original index lives in the low half, so keys are unique even when Morton codes collide.
    keys_.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const Vec3 c = boxes[i].center();
        const std::uint32_t code =
            morton_code(quantize(c.x, centroids.min.x, scale.x), quantize(c.y, centroids.min.y, scale.y),
                        quantize(c.z, centroids.min.z, scale.z));
        keys_[i] = (std::uint64_t{code} << 32) | i;
    }

    // LSD radix sort on the 30 code bits. Stable, so equal codes stay in index order.
    constexpr int digit_bits = 10;
    constexpr std::uint64_t digit_mask = (1u << digit_bits) - 1;
    scratch_.resize(n);
    for (int shift = 32; shift < 62; shift += digit_bits) {
        std::array<std::uint32_t, 1u << digit_bits> offsets{};
        for (const std::uint64_t key : keys_) {
            ++offsets[(key >> shift) & digit_mask];
        }
        std::uint32_t sum = 0;
        for (std::uint32_t& offset : offsets) {
            sum += std::exchange(offset, sum);
        }
        for (const std::uint64_t key : keys_) {
            scratch_[offsets[(key >> shift) & digit_mask]++] = key;
        }
        keys_.swap(scratch_);
    }

    // Re-key by sorted position: the hierarchy builder only needs keys to be
    // unique and ordered, and the leaf index is cheaper to carry than the body id.
    leaf_ids_.resize(n);
    leaf_boxes_.resize(n);
    for (std::size_t k = 0; k < n; ++k) {
        const auto id = static_cast<std::uint32_t>(keys_[k]);
        leaf_ids_[k] = id;
        leaf_boxes_[k] = boxes[id];
        keys_[k] = (keys_[k] & 0xFFFF'FFFF'0000'0000ull) | k;
    }
}

void Bvh::build_hierarchy() {
    const auto n = static_cast<std::int64_t>(keys_.size());
    const auto internal_count = static_cast<std::size_t>(n - 1);
    nodes_.resize(internal_count);
    parents_.resize(internal_count + static_cast<std::size_t>(n));
    parents_[0] = 0;

    auto prefix = [&](std::int64_t i, std::int64_t j) -> int {
        if (j < 0 || j >= n) {
            return -1;
        }
        return std::countl_zero(keys_[static_cast<std::size_t>(i)] ^ keys_[static_cast<std::size_t>(j)]);
    };

    // Each internal node is found independently from its position in the key
    // array, which is what makes the construction parallel on a GPU.
    for (std::int64_t i = 0; i < n - 1; ++i) {
        const std::int64_t d = prefix(i, i + 1) > prefix(i, i - 1) ? 1 : -1;

        const int prefix_min = prefix(i, i - d);
        std::int64_t length_max = 2;
        while (prefix(i, i + length_max * d) > prefix_min) {
            length_max *= 2;
        }
        std::int64_t length = 0;
        for (std::int64_t t = length_max / 2; t >= 1; t /= 2) {
            if (prefix(i, i + (length + t) * d) > prefix_min) {
                length += t;
            }
        }
        const std::int64_t j = i + length * d;

        const int prefix_node = prefix(i, j);
        std::int64_t split = 0;
        std::int64_t step = length;
        do {
            step = (step + 1) / 2;
            if (prefix(i, i + (split + step) * d) > prefix_node) {
                split += step;
            }
        } while (step > 1);
        const std::int64_t gamma = i + split * d + std::min<std::int64_t>(d, 0);

        const auto first = static_cast<std::uint32_t>(std::min(i, j));
        const auto last = static_cast<std::uint32_t>(std::max(i, j));
        const auto g = static_cast<std::uint32_t>(gamma);

        Node& node = nodes_[static_cast<std::size_t>(i)];
        node.left = first == g ? (g | leaf_flag) : g;
        node.right = last == g + 1 ? ((g + 1) | leaf_flag) : g + 1;
        node.last = last;

        const auto self = static_cast<std::uint32_t>(i);
        for (const std::uint32_t child : {node.left, node.right}) {
            const std::size_t slot = is_leaf(child) ? internal_count + leaf_index(child) : child;
            parents_[slot] = self;
        }
    }
}

// Bottom-up: the second child to reach a node computes its bounds, so every node
// is merged exactly once without recursion.
void Bvh::fit_bounds() {
    const std::size_t internal_count = nodes_.size();
    visits_.assign(internal_count, 0);

    auto bounds_of = [this](std::uint32_t child) -> const Aabb& {
        return is_leaf(child) ? leaf_boxes_[leaf_index(child)] : nodes_[child].box;
    };

    for (std::size_t k = 0; k < leaf_boxes_.size(); ++k) {
        std::uint32_t p = parents_[internal_count + k];
        while (visits_[p]++ != 0) {
            Node& node = nodes_[p];
            node.box = merge(bounds_of(node.left), bounds_of(node.right));
            if (p == 0) {
                break;
            }
            p = parents_[p];
        }
    }
}

void Bvh::collect_pairs(std::vector<Pair>& out) const {
    // Keys are unique 64-bit values and every level consumes at least one bit of
    // shared prefix, so the tree is at most 64 deep.
    std::array<std::uint32_t, 128> stack;

    const auto leaf_count = static_cast<std::uint32_t>(leaf_boxes_.size());
    for (std::uint32_t k = 0; k + 1 < leaf_count; ++k) {
        const Aabb query = leaf_boxes_[k];
        std::size_t top = 0;
        stack[top++] = 0;

        while (top > 0) {
            const Node& node = nodes_[stack[--top]];
            for (const std::uint32_t child : {node.left, node.right}) {
                if (is_leaf(child)) {
                    // Only leaves after k, so each pair is reported once.
                    const std::uint32_t m = leaf_index(child);
                    if (m > k && overlaps(query, leaf_boxes_[m])) {
                        out.push_back(ordered_pair(leaf_ids_[k], leaf_ids_[m]));
                    }
                } else if (nodes_[child].last > k && overlaps(query, nodes_[child].box)) {
                    assert(top < stack.size());
                    stack[top++] = child;
                }
            }
        }
    }
}

} // namespace idem
