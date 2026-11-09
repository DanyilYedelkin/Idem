#include <idem/recording.hpp>
#include <idem/replay.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <vector>

using namespace idem;

namespace {

SceneConfig test_scene() {
    SceneConfig config;
    config.body_count = 300;
    config.seed = 77;
    return config;
}

const Recording& sample_recording() {
    static const Recording recording = record(test_scene(), SimulationParams{}, BroadPhaseKind::bvh, 90);
    return recording;
}

// Copies `source`, letting `edit` rewrite the commands of each tick.
template <class Edit>
Recording rewrite(const Recording& source, Edit edit) {
    Recording copy;
    copy.scene = source.scene;
    copy.params = source.params;
    copy.broad_phase = source.broad_phase;
    copy.initial_hash = source.initial_hash;
    for (std::size_t t = 0; t < source.tick_count(); ++t) {
        const auto original = source.commands(t);
        std::vector<Command> commands(original.begin(), original.end());
        edit(t, commands);
        copy.append_tick(commands, source.hash_after(t));
    }
    return copy;
}

} // namespace

TEST_CASE("recording stores every tick", "[recording]") {
    const Recording& rec = sample_recording();
    CHECK(rec.tick_count() == 90);
    CHECK(rec.total_commands() > 0);
    CHECK(rec.scene == test_scene());
}

TEST_CASE("serialisation round-trips", "[recording]") {
    const Recording& rec = sample_recording();
    const auto bytes = rec.serialize();
    const auto parsed = Recording::deserialize(bytes);
    REQUIRE(parsed.has_value());
    CHECK(*parsed == rec);
    CHECK(parsed->serialize() == bytes);
}

TEST_CASE("recordings survive a trip through the file system", "[recording]") {
    const auto path = std::filesystem::temp_directory_path() / "idem_recording_test.idem";
    REQUIRE(save(sample_recording(), path).has_value());
    const auto loaded = load(path);
    std::filesystem::remove(path);
    REQUIRE(loaded.has_value());
    CHECK(*loaded == sample_recording());

    CHECK(load(path).error() == RecordingError::io_error);
}

TEST_CASE("replay verifies with every broad-phase", "[recording][replay]") {
    for (const BroadPhaseKind kind : all_broad_phases) {
        const ReplayReport report = replay(sample_recording(), kind);
        CHECK(report.ok());
        CHECK(report.ticks_verified == 90);
        CHECK(report.final_hash == sample_recording().hash_after(89));
    }
}

TEST_CASE("replay pinpoints the first tick with a different input", "[recording][replay]") {
    const Recording& rec = sample_recording();
    std::size_t target = 40;
    while (rec.commands(target).empty()) {
        ++target;
    }

    const Recording tampered = rewrite(rec, [&](std::size_t t, std::vector<Command>& commands) {
        if (t == target) {
            commands.front().impulse.x += 0.001f;
        }
    });
    const ReplayReport report = replay(tampered, BroadPhaseKind::uniform_grid);
    REQUIRE_FALSE(report.ok());
    CHECK(report.divergence->tick == target + 1);
    CHECK(report.ticks_verified == target);
}

TEST_CASE("replay detects a different initial state", "[recording][replay]") {
    Recording rec = sample_recording();
    rec.scene.seed += 1;
    const ReplayReport report = replay(rec, BroadPhaseKind::uniform_grid);
    REQUIRE_FALSE(report.ok());
    CHECK(report.divergence->tick == 0);
}

TEST_CASE("corrupt files are rejected", "[recording]") {
    const auto bytes = sample_recording().serialize();

    SECTION("every truncation") {
        for (std::size_t length = 0; length < bytes.size(); length += 7) {
            const auto result = Recording::deserialize(std::span{bytes}.first(length));
            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error() == RecordingError::truncated);
        }
    }

    SECTION("bad magic") {
        auto copy = bytes;
        copy[0] = std::byte{'X'};
        CHECK(Recording::deserialize(copy).error() == RecordingError::bad_magic);
    }

    SECTION("unknown version") {
        auto copy = bytes;
        copy[4] = std::byte{99};
        CHECK(Recording::deserialize(copy).error() == RecordingError::unsupported_version);
    }

    SECTION("trailing data") {
        auto copy = bytes;
        copy.push_back(std::byte{0});
        CHECK(Recording::deserialize(copy).error() == RecordingError::trailing_data);
    }

    SECTION("command for a body that does not exist") {
        const Recording bad = rewrite(sample_recording(), [](std::size_t t, std::vector<Command>& commands) {
            if (t == 0) {
                commands.push_back({test_scene().body_count, {}});
            }
        });
        CHECK(Recording::deserialize(bad.serialize()).error() == RecordingError::invalid_field);
    }

    SECTION("absurd body count") {
        Recording bad = sample_recording();
        bad.scene.body_count = 0xFFFF'FFFF;
        CHECK(Recording::deserialize(bad.serialize()).error() == RecordingError::invalid_field);
    }
}
