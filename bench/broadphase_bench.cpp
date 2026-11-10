#include <idem/broadphase/broadphase.hpp>
#include <idem/scene.hpp>
#include <idem/simulation.hpp>
#include <idem/state_hash.hpp>

#include <benchmark/benchmark.h>

#include <string>
#include <utility>
#include <vector>

namespace {

using namespace idem;

constexpr std::uint64_t bench_seed = 0xBE7C4;

// Boxes from a scene that has run for a while, so the distribution includes the
// floor pile-up and resting contacts, not just uniform noise.
std::vector<Aabb> settled_boxes(std::uint32_t count) {
    SceneConfig config;
    config.body_count = count;
    config.seed = bench_seed;
    Simulation sim{make_scene(config), SimulationParams{}, BroadPhaseKind::uniform_grid};
    for (int t = 0; t < 120; ++t) {
        sim.step({});
    }

    const Bodies& bodies = sim.bodies();
    std::vector<Aabb> boxes(bodies.size());
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        boxes[i] = Aabb::around_sphere(bodies.position[i], bodies.radius[i]);
    }
    return boxes;
}

// The same scene with every hundredth body inflated to a 4 m radius, the way a
// few vehicles or buildings sit among many small entities. A uniform grid has to
// size its cells for the largest object; a BVH does not care.
std::vector<Aabb> with_large_outliers(std::vector<Aabb> boxes) {
    for (std::size_t i = 0; i < boxes.size(); i += 100) {
        boxes[i] = Aabb::around_sphere(boxes[i].center(), 4.0f);
    }
    return boxes;
}

enum class Distribution { uniform_sizes, large_outliers };

template <Distribution D>
const std::vector<Aabb>& benchmark_boxes(std::uint32_t count) {
    static std::vector<std::pair<std::uint32_t, std::vector<Aabb>>> cache;
    for (const auto& [n, boxes] : cache) {
        if (n == count) {
            return boxes;
        }
    }
    auto boxes = settled_boxes(count);
    if constexpr (D == Distribution::large_outliers) {
        boxes = with_large_outliers(std::move(boxes));
    }
    return cache.emplace_back(count, std::move(boxes)).second;
}

template <BroadPhase T, Distribution D = Distribution::uniform_sizes>
void bm_find_pairs(benchmark::State& state) {
    const auto& boxes = benchmark_boxes<D>(static_cast<std::uint32_t>(state.range(0)));
    T broad_phase;
    std::vector<Pair> pairs;
    for (auto _ : state) {
        broad_phase.find_pairs(boxes, pairs);
        benchmark::DoNotOptimize(pairs.data());
        benchmark::ClobberMemory();
    }
    state.counters["pairs"] = static_cast<double>(pairs.size());
    state.SetComplexityN(state.range(0));
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

void bm_step(benchmark::State& state) {
    SceneConfig config;
    config.body_count = static_cast<std::uint32_t>(state.range(0));
    config.seed = bench_seed;
    Simulation sim{make_scene(config), SimulationParams{}, static_cast<BroadPhaseKind>(state.range(1))};
    for (int t = 0; t < 60; ++t) {
        sim.step({});
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(sim.step({}));
    }
    state.SetLabel(std::string{to_string(sim.broad_phase())});
}

void bm_state_hash(benchmark::State& state) {
    SceneConfig config;
    config.body_count = static_cast<std::uint32_t>(state.range(0));
    const Scene scene = make_scene(config);
    for (auto _ : state) {
        benchmark::DoNotOptimize(idem::hash_state(scene.bodies, 0));
    }
    state.SetBytesProcessed(state.iterations() * state.range(0) * 2 *
                            static_cast<std::int64_t>(sizeof(Vec3)));
}

void scaling_args(benchmark::internal::Benchmark* b, std::int64_t max_count) {
    for (const std::int64_t n : {1'000, 2'000, 5'000, 10'000, 20'000, 50'000, 100'000}) {
        if (n <= max_count) {
            b->Arg(n);
        }
    }
}

} // namespace

BENCHMARK(bm_find_pairs<BruteForce>)->Name("broadphase/brute")->Apply([](auto* b) {
    scaling_args(b, 20'000);
}) -> Complexity(benchmark::oNSquared) -> Unit(benchmark::kMillisecond);
BENCHMARK(bm_find_pairs<UniformGrid>)->Name("broadphase/grid")->Apply([](auto* b) {
    scaling_args(b, 100'000);
}) -> Complexity(benchmark::oN) -> Unit(benchmark::kMillisecond);
BENCHMARK(bm_find_pairs<Bvh>)->Name("broadphase/bvh")->Apply([](auto* b) {
    scaling_args(b, 100'000);
}) -> Complexity(benchmark::oNLogN) -> Unit(benchmark::kMillisecond);

BENCHMARK(bm_find_pairs<UniformGrid, Distribution::large_outliers>)
    ->Name("broadphase_outliers/grid")
    ->Apply([](auto* b) { scaling_args(b, 100'000); }) -> Unit(benchmark::kMillisecond);
BENCHMARK(bm_find_pairs<Bvh, Distribution::large_outliers>)
    ->Name("broadphase_outliers/bvh")
    ->Apply([](auto* b) { scaling_args(b, 100'000); }) -> Unit(benchmark::kMillisecond);

BENCHMARK(bm_step)
    ->Name("step")
    ->ArgsProduct({{10'000, 100'000},
                   {static_cast<std::int64_t>(BroadPhaseKind::uniform_grid),
                    static_cast<std::int64_t>(BroadPhaseKind::bvh)}})
    ->ArgNames({"bodies", "broadphase"})
    ->Unit(benchmark::kMillisecond);

BENCHMARK(bm_state_hash)->Name("state_hash")->Arg(100'000)->Unit(benchmark::kMicrosecond);
