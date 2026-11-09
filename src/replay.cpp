#include <idem/replay.hpp>

#include <idem/input.hpp>

namespace idem {

std::uint64_t input_seed(std::uint64_t scene_seed) noexcept {
    return SplitMix64{scene_seed ^ 0x1D3A'5EED'0000'0001ull}.next();
}

Recording record(const SceneConfig& scene, const SimulationParams& params, BroadPhaseKind broad_phase,
                 std::uint32_t ticks, const StepObserver& observer) {
    Simulation sim{make_scene(scene), params, broad_phase};
    InputScript inputs{input_seed(scene.seed), scene.body_count};

    Recording rec;
    rec.scene = scene;
    rec.params = params;
    rec.broad_phase = broad_phase;
    rec.initial_hash = sim.state_hash();

    for (std::uint32_t t = 0; t < ticks; ++t) {
        const auto commands = inputs.next_tick();
        const StepStats stats = sim.step(commands);
        rec.append_tick(commands, sim.state_hash());
        if (observer) {
            observer(sim, stats);
        }
    }
    return rec;
}

ReplayReport replay(const Recording& recording, BroadPhaseKind broad_phase, const StepObserver& observer) {
    Simulation sim{make_scene(recording.scene), recording.params, broad_phase};

    ReplayReport report;
    report.final_hash = sim.state_hash();
    if (report.final_hash != recording.initial_hash) {
        report.divergence = Divergence{0, recording.initial_hash, report.final_hash};
        return report;
    }

    for (std::size_t t = 0; t < recording.tick_count(); ++t) {
        const StepStats stats = sim.step(recording.commands(t));
        if (observer) {
            observer(sim, stats);
        }
        report.final_hash = sim.state_hash();
        if (report.final_hash != recording.hash_after(t)) {
            report.divergence = Divergence{sim.tick(), recording.hash_after(t), report.final_hash};
            return report;
        }
        report.ticks_verified = sim.tick();
    }
    return report;
}

} // namespace idem
