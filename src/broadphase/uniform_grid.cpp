#include <idem/broadphase/uniform_grid.hpp>

#include <array>
#include <cmath>

namespace idem {

namespace {

struct CellOffset {
    int dx, dy, dz;
};

// Half of the 26-neighbourhood: each unordered pair of adjacent cells is visited once.
constexpr std::array<CellOffset, 13> forward_neighbours = [] {
    std::array<CellOffset, 13> offsets{};
    std::size_t count = 0;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dz > 0 || (dz == 0 && dy > 0) || (dz == 0 && dy == 0 && dx > 0)) {
                    offsets[count++] = {dx, dy, dz};
                }
            }
        }
    }
    return offsets;
}();

// Upper bound on cells per box; keeps memory linear in n for sparse scenes.
constexpr float max_cells_per_box = 8.0f;

} // namespace

void UniformGrid::find_pairs(std::span<const Aabb> boxes, std::vector<Pair>& out) {
    out.clear();
    const std::size_t n = boxes.size();
    if (n < 2) {
        return;
    }

    Aabb world = Aabb::empty();
    float largest = 0.0f;
    for (const Aabb& box : boxes) {
        world.expand(box);
        const Vec3 e = box.extent();
        largest = std::max({largest, e.x, e.y, e.z});
    }

    // Overlapping boxes that fit in a cell have centres less than one cell apart on
    // every axis. The slack keeps that true after rounding in the cell computation.
    float cell = std::max(largest * 1.001f, std::numeric_limits<float>::min());
    const Vec3 span = world.extent();
    const float budget = max_cells_per_box * static_cast<float>(n);
    auto cells_along = [&](float length) { return std::floor(length / cell) + 1.0f; };
    while (cells_along(span.x) * cells_along(span.y) * cells_along(span.z) > budget) {
        cell *= 2.0f;
    }

    const auto nx = static_cast<std::uint32_t>(cells_along(span.x));
    const auto ny = static_cast<std::uint32_t>(cells_along(span.y));
    const auto nz = static_cast<std::uint32_t>(cells_along(span.z));
    const std::size_t cell_count = std::size_t{nx} * ny * nz;
    const float inv_cell = 1.0f / cell;

    auto coord = [inv_cell](float v, float lo, std::uint32_t dim) {
        const auto c = static_cast<std::uint32_t>((v - lo) * inv_cell);
        return std::min(c, dim - 1);
    };

    cell_of_.resize(n);
    cell_start_.assign(cell_count + 1, 0);
    for (std::size_t i = 0; i < n; ++i) {
        const Vec3 c = boxes[i].center();
        const std::uint32_t x = coord(c.x, world.min.x, nx);
        const std::uint32_t y = coord(c.y, world.min.y, ny);
        const std::uint32_t z = coord(c.z, world.min.z, nz);
        const std::uint32_t id = x + nx * (y + ny * z);
        cell_of_[i] = id;
        ++cell_start_[id + 1];
    }
    for (std::size_t c = 0; c < cell_count; ++c) {
        cell_start_[c + 1] += cell_start_[c];
    }

    // Scatter into cell order. Boxes are copied so the pair tests read contiguous memory.
    sorted_ids_.resize(n);
    sorted_boxes_.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint32_t slot = cell_start_[cell_of_[i]]++;
        sorted_ids_[slot] = static_cast<std::uint32_t>(i);
        sorted_boxes_[slot] = boxes[i];
    }
    // Every start now points at the next cell's start.
    std::shift_right(cell_start_.begin(), cell_start_.end(), 1);
    cell_start_[0] = 0;

    auto test_ranges = [&](std::uint32_t a_begin, std::uint32_t a_end, std::uint32_t b_begin,
                           std::uint32_t b_end) {
        for (std::uint32_t i = a_begin; i < a_end; ++i) {
            const Aabb box = sorted_boxes_[i];
            for (std::uint32_t j = b_begin; j < b_end; ++j) {
                if (overlaps(box, sorted_boxes_[j])) {
                    out.push_back(ordered_pair(sorted_ids_[i], sorted_ids_[j]));
                }
            }
        }
    };

    for (std::uint32_t z = 0; z < nz; ++z) {
        for (std::uint32_t y = 0; y < ny; ++y) {
            for (std::uint32_t x = 0; x < nx; ++x) {
                const std::uint32_t id = x + nx * (y + ny * z);
                const std::uint32_t begin = cell_start_[id];
                const std::uint32_t end = cell_start_[id + 1];
                if (begin == end) {
                    continue;
                }

                for (std::uint32_t i = begin; i < end; ++i) {
                    test_ranges(i, i + 1, i + 1, end);
                }

                for (const auto [dx, dy, dz] : forward_neighbours) {
                    const auto ox = static_cast<std::int64_t>(x) + dx;
                    const auto oy = static_cast<std::int64_t>(y) + dy;
                    const auto oz = static_cast<std::int64_t>(z) + dz;
                    if (ox < 0 || oy < 0 || ox >= nx || oy >= ny || oz >= nz) {
                        continue;
                    }
                    const auto other = static_cast<std::uint32_t>(ox + nx * (oy + std::int64_t{ny} * oz));
                    test_ranges(begin, end, cell_start_[other], cell_start_[other + 1]);
                }
            }
        }
    }
}

} // namespace idem
