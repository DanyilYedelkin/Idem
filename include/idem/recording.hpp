#pragma once

#include <idem/broadphase/broadphase.hpp>
#include <idem/input.hpp>
#include <idem/scene.hpp>
#include <idem/simulation.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace idem {

enum class RecordingError : std::uint8_t {
    io_error,
    bad_magic,
    unsupported_version,
    truncated,
    invalid_field,
    trailing_data,
};

[[nodiscard]] std::string_view to_string(RecordingError error) noexcept;

// Everything needed to reproduce a run: the initial conditions, the inputs of
// every tick, and the state hash after every tick to verify against.
class Recording {
public:
    static constexpr std::uint16_t format_version = 1;

    SceneConfig scene;
    SimulationParams params;
    BroadPhaseKind broad_phase = BroadPhaseKind::uniform_grid;
    std::uint64_t initial_hash = 0;

    [[nodiscard]] std::size_t tick_count() const noexcept { return hashes_.size(); }
    [[nodiscard]] std::span<const Command> commands(std::size_t tick) const;
    [[nodiscard]] std::uint64_t hash_after(std::size_t tick) const { return hashes_.at(tick); }
    [[nodiscard]] std::size_t total_commands() const noexcept { return commands_.size(); }

    void append_tick(std::span<const Command> commands, std::uint64_t hash_after);

    [[nodiscard]] std::vector<std::byte> serialize() const;
    [[nodiscard]] static std::expected<Recording, RecordingError>
    deserialize(std::span<const std::byte> bytes);

    friend bool operator==(const Recording&, const Recording&) = default;

private:
    std::vector<Command> commands_;
    std::vector<std::uint32_t> command_offsets_{0}; // tick t owns [offsets[t], offsets[t + 1])
    std::vector<std::uint64_t> hashes_;
};

[[nodiscard]] std::expected<void, RecordingError> save(const Recording& recording,
                                                       const std::filesystem::path& path);

[[nodiscard]] std::expected<Recording, RecordingError> load(const std::filesystem::path& path);

} // namespace idem
