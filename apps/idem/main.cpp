#include "options.hpp"

#include <idem/recording.hpp>
#include <idem/replay.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <expected>
#include <filesystem>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace idem;
using cli::Options;

constexpr std::string_view usage = R"(usage:
  idem record [--bodies N] [--ticks N] [--seed N] [--broadphase brute|grid|bvh] [--out FILE]
  idem replay FILE [--broadphase brute|grid|bvh]
  idem info FILE
  idem diff FILE_A FILE_B
)";

enum ExitCode : int { exit_ok = 0, exit_mismatch = 1, exit_usage = 2, exit_error = 3 };

class Stopwatch {
public:
    [[nodiscard]] double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
    }

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point start_ = Clock::now();
};

struct RunTotals {
    std::uint64_t ticks = 0;
    std::uint64_t contacts = 0;

    StepObserver observer() {
        return [this](const Simulation&, const StepStats& stats) {
            ++ticks;
            contacts += stats.contacts;
        };
    }
};

int usage_error(std::string_view message) {
    std::println(stderr, "idem: {}\n\n{}", message, usage);
    return exit_usage;
}

std::expected<Recording, int> load_or_report(std::string_view path) {
    auto recording = load(std::filesystem::path{path});
    if (!recording) {
        std::println(stderr, "idem: {}: {}", path, to_string(recording.error()));
        return std::unexpected{exit_error};
    }
    return std::move(*recording);
}

void print_timing(const RunTotals& totals, double elapsed_ms) {
    const double ticks = static_cast<double>(std::max<std::uint64_t>(totals.ticks, 1));
    std::println("  {:.3f} ms/tick, {:.0f} contacts/tick", elapsed_ms / ticks,
                 static_cast<double>(totals.contacts) / ticks);
}

int run_record(const Options& options) {
    if (auto allowed = options.allow_only({"bodies", "ticks", "seed", "broadphase", "out"}); !allowed) {
        return usage_error(allowed.error());
    }
    if (!options.positional().empty()) {
        return usage_error("record takes no positional arguments");
    }

    const auto bodies = options.get<std::uint32_t>("bodies", 10'000);
    const auto ticks = options.get<std::uint32_t>("ticks", 600);
    const auto seed = options.get<std::uint64_t>("seed", 1);
    const auto broad_phase = parse_broad_phase(options.get("broadphase", "grid"));
    if (!bodies) {
        return usage_error(bodies.error());
    }
    if (!ticks) {
        return usage_error(ticks.error());
    }
    if (!seed) {
        return usage_error(seed.error());
    }
    if (!broad_phase) {
        return usage_error("unknown broad-phase");
    }
    const std::filesystem::path out{options.get("out", "run.idem")};

    SceneConfig scene;
    scene.body_count = *bodies;
    scene.seed = *seed;

    RunTotals totals;
    const Stopwatch timer;
    const Recording recording = record(scene, SimulationParams{}, *broad_phase, *ticks, totals.observer());
    const double elapsed = timer.elapsed_ms();

    if (auto saved = save(recording, out); !saved) {
        std::println(stderr, "idem: {}: {}", out.string(), to_string(saved.error()));
        return exit_error;
    }

    const std::uint64_t final_hash = recording.tick_count() > 0
                                         ? recording.hash_after(recording.tick_count() - 1)
                                         : recording.initial_hash;
    std::println("recorded {} ticks of {} bodies with {} -> {}", recording.tick_count(), scene.body_count,
                 to_string(*broad_phase), out.string());
    print_timing(totals, elapsed);
    std::println("  final hash {:#018x}", final_hash);
    return exit_ok;
}

int run_replay(const Options& options) {
    if (auto allowed = options.allow_only({"broadphase"}); !allowed) {
        return usage_error(allowed.error());
    }
    if (options.positional().size() != 1) {
        return usage_error("replay expects exactly one file");
    }
    const auto recording = load_or_report(options.positional()[0]);
    if (!recording) {
        return recording.error();
    }

    BroadPhaseKind broad_phase = recording->broad_phase;
    if (options.has("broadphase")) {
        const auto parsed = parse_broad_phase(options.get("broadphase", ""));
        if (!parsed) {
            return usage_error("unknown broad-phase");
        }
        broad_phase = *parsed;
    }

    RunTotals totals;
    const Stopwatch timer;
    const ReplayReport report = replay(*recording, broad_phase, totals.observer());
    const double elapsed = timer.elapsed_ms();

    if (!report.ok()) {
        const Divergence& d = *report.divergence;
        std::println("DIVERGED at tick {} using {}", d.tick, to_string(broad_phase));
        std::println("  expected {:#018x}", d.expected_hash);
        std::println("  actual   {:#018x}", d.actual_hash);
        return exit_mismatch;
    }
    std::println("OK: {} ticks verified with {}", report.ticks_verified, to_string(broad_phase));
    print_timing(totals, elapsed);
    std::println("  final hash {:#018x}", report.final_hash);
    return exit_ok;
}

int run_info(const Options& options) {
    if (auto allowed = options.allow_only({}); !allowed) {
        return usage_error(allowed.error());
    }
    if (options.positional().size() != 1) {
        return usage_error("info expects exactly one file");
    }
    const auto recording = load_or_report(options.positional()[0]);
    if (!recording) {
        return recording.error();
    }

    const Recording& r = *recording;
    const SceneConfig& s = r.scene;
    const SimulationParams& p = r.params;
    std::println("format       v{}", Recording::format_version);
    std::println("bodies       {} (radius {}..{}, seed {})", s.body_count, s.min_radius, s.max_radius,
                 s.seed);
    std::println("world side   {}", world_side_length(s));
    std::println("dt           {} s", p.dt);
    std::println("gravity      ({}, {}, {})", p.gravity.x, p.gravity.y, p.gravity.z);
    std::println("restitution  {}", p.restitution);
    std::println("recorded on  {}", to_string(r.broad_phase));
    std::println("ticks        {} ({} commands)", r.tick_count(), r.total_commands());
    std::println("hash t=0     {:#018x}", r.initial_hash);
    if (r.tick_count() > 0) {
        std::println("hash t={:<5} {:#018x}", r.tick_count(), r.hash_after(r.tick_count() - 1));
    }
    return exit_ok;
}

int run_diff(const Options& options) {
    if (auto allowed = options.allow_only({}); !allowed) {
        return usage_error(allowed.error());
    }
    if (options.positional().size() != 2) {
        return usage_error("diff expects two files");
    }
    const auto a = load_or_report(options.positional()[0]);
    if (!a) {
        return a.error();
    }
    const auto b = load_or_report(options.positional()[1]);
    if (!b) {
        return b.error();
    }

    if (a->scene != b->scene || a->params != b->params) {
        std::println("recordings start from different configurations");
        return exit_mismatch;
    }
    if (a->initial_hash != b->initial_hash) {
        std::println("initial state differs: {:#018x} vs {:#018x}", a->initial_hash, b->initial_hash);
        return exit_mismatch;
    }

    const std::size_t common = std::min(a->tick_count(), b->tick_count());
    for (std::size_t t = 0; t < common; ++t) {
        if (!std::ranges::equal(a->commands(t), b->commands(t))) {
            std::println("inputs differ at tick {}", t + 1);
            return exit_mismatch;
        }
        if (a->hash_after(t) != b->hash_after(t)) {
            std::println("state diverges at tick {}: {:#018x} vs {:#018x}", t + 1, a->hash_after(t),
                         b->hash_after(t));
            return exit_mismatch;
        }
    }
    if (a->tick_count() != b->tick_count()) {
        std::println("identical for {} ticks, but lengths differ ({} vs {})", common, a->tick_count(),
                     b->tick_count());
        return exit_mismatch;
    }
    std::println("identical: {} ticks", common);
    return exit_ok;
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        std::print("{}", usage);
        return args.empty() ? exit_usage : exit_ok;
    }

    const auto options = Options::parse(std::span{args}.subspan(1));
    if (!options) {
        return usage_error(options.error());
    }

    const std::string_view command = args[0];
    try {
        if (command == "record") {
            return run_record(*options);
        }
        if (command == "replay") {
            return run_replay(*options);
        }
        if (command == "info") {
            return run_info(*options);
        }
        if (command == "diff") {
            return run_diff(*options);
        }
    } catch (const std::exception& e) {
        std::println(stderr, "idem: {}", e.what());
        return exit_error;
    }
    return usage_error("unknown command '" + std::string{command} + "'");
}
