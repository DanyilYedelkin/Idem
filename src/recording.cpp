#include <idem/recording.hpp>

#include "byte_stream.hpp"

#include <array>
#include <cmath>
#include <fstream>
#include <limits>

// File layout, version 1. All values little-endian, floats as IEEE-754 binary32.
//
//   char[4]  magic "IDEM"
//   u16      format version
//   u16      reserved, zero
//   scene    u32 body_count, u64 seed, f32 min_radius, f32 max_radius,
//            f32 max_speed, f32 fill_ratio
//   params   f32 dt, f32 gravity[3], f32 restitution
//   u8       broad-phase used while recording (informational)
//   u64      state hash at tick 0
//   u32      tick count
//   per tick u32 command count, then {u32 body, f32 impulse[3]} per command,
//            then u64 state hash after the tick
//
// The initial state is regenerated from the scene config rather than stored,
// which keeps files small and means a replay also verifies scene generation.

namespace idem {

namespace {

constexpr std::array<std::byte, 4> magic{std::byte{'I'}, std::byte{'D'}, std::byte{'E'}, std::byte{'M'}};
constexpr std::size_t command_size = sizeof(std::uint32_t) + 3 * sizeof(float);
constexpr std::uint32_t max_body_count = 1u << 26;

void write_vec3(detail::ByteWriter& out, Vec3 v) {
    out.write(v.x);
    out.write(v.y);
    out.write(v.z);
}

[[nodiscard]] bool read_vec3(detail::ByteReader& in, Vec3& v) noexcept {
    return in.read(v.x) && in.read(v.y) && in.read(v.z);
}

[[nodiscard]] bool is_valid(const SceneConfig& s) noexcept {
    const bool finite = std::isfinite(s.min_radius) && std::isfinite(s.max_radius) &&
                        std::isfinite(s.max_speed) && std::isfinite(s.fill_ratio);
    return finite && s.body_count <= max_body_count && s.min_radius > 0.0f && s.min_radius <= s.max_radius &&
           s.max_speed >= 0.0f && s.fill_ratio > 0.0f && s.fill_ratio <= 1.0f;
}

[[nodiscard]] bool is_valid(const SimulationParams& p) noexcept {
    return std::isfinite(p.dt) && p.dt > 0.0f && std::isfinite(p.gravity.x) && std::isfinite(p.gravity.y) &&
           std::isfinite(p.gravity.z) && std::isfinite(p.restitution) && p.restitution >= 0.0f &&
           p.restitution <= 1.0f;
}

} // namespace

std::string_view to_string(RecordingError error) noexcept {
    switch (error) {
    case RecordingError::io_error:
        return "I/O error";
    case RecordingError::bad_magic:
        return "not an Idem recording";
    case RecordingError::unsupported_version:
        return "unsupported format version";
    case RecordingError::truncated:
        return "file is truncated";
    case RecordingError::invalid_field:
        return "file contains an invalid value";
    case RecordingError::trailing_data:
        return "unexpected data after the last tick";
    }
    return "unknown error";
}

std::span<const Command> Recording::commands(std::size_t tick) const {
    const std::uint32_t begin = command_offsets_.at(tick);
    const std::uint32_t end = command_offsets_.at(tick + 1);
    return std::span{commands_}.subspan(begin, end - begin);
}

void Recording::append_tick(std::span<const Command> commands, std::uint64_t hash_after) {
    commands_.insert(commands_.end(), commands.begin(), commands.end());
    command_offsets_.push_back(static_cast<std::uint32_t>(commands_.size()));
    hashes_.push_back(hash_after);
}

std::vector<std::byte> Recording::serialize() const {
    detail::ByteWriter out;
    out.write_bytes(magic);
    out.write(format_version);
    out.write(std::uint16_t{0});

    out.write(scene.body_count);
    out.write(scene.seed);
    out.write(scene.min_radius);
    out.write(scene.max_radius);
    out.write(scene.max_speed);
    out.write(scene.fill_ratio);

    out.write(params.dt);
    write_vec3(out, params.gravity);
    out.write(params.restitution);

    out.write(std::to_underlying(broad_phase));
    out.write(initial_hash);

    out.write(static_cast<std::uint32_t>(tick_count()));
    for (std::size_t t = 0; t < tick_count(); ++t) {
        const auto tick_commands = commands(t);
        out.write(static_cast<std::uint32_t>(tick_commands.size()));
        for (const Command& cmd : tick_commands) {
            out.write(cmd.body);
            write_vec3(out, cmd.impulse);
        }
        out.write(hashes_[t]);
    }
    return std::move(out).take();
}

std::expected<Recording, RecordingError> Recording::deserialize(std::span<const std::byte> bytes) {
    detail::ByteReader in{bytes};
    Recording rec;

    std::array<std::byte, 4> file_magic{};
    if (!in.read_bytes(file_magic)) {
        return std::unexpected{RecordingError::truncated};
    }
    if (file_magic != magic) {
        return std::unexpected{RecordingError::bad_magic};
    }

    std::uint16_t version = 0;
    std::uint16_t reserved = 0;
    if (!in.read(version) || !in.read(reserved)) {
        return std::unexpected{RecordingError::truncated};
    }
    if (version != format_version) {
        return std::unexpected{RecordingError::unsupported_version};
    }

    SceneConfig& s = rec.scene;
    SimulationParams& p = rec.params;
    std::uint8_t broad_phase = 0;
    std::uint32_t tick_count = 0;
    const bool header_ok = in.read(s.body_count) && in.read(s.seed) && in.read(s.min_radius) &&
                           in.read(s.max_radius) && in.read(s.max_speed) && in.read(s.fill_ratio) &&
                           in.read(p.dt) && read_vec3(in, p.gravity) && in.read(p.restitution) &&
                           in.read(broad_phase) && in.read(rec.initial_hash) && in.read(tick_count);
    if (!header_ok) {
        return std::unexpected{RecordingError::truncated};
    }
    if (reserved != 0 || !is_valid(s) || !is_valid(p) || broad_phase >= all_broad_phases.size()) {
        return std::unexpected{RecordingError::invalid_field};
    }
    rec.broad_phase = static_cast<BroadPhaseKind>(broad_phase);

    // Size checks before every reserve: a corrupt count must not turn into a huge allocation.
    constexpr std::size_t min_tick_size = sizeof(std::uint32_t) + sizeof(std::uint64_t);
    if (tick_count > in.remaining() / min_tick_size) {
        return std::unexpected{RecordingError::truncated};
    }
    rec.hashes_.reserve(tick_count);
    rec.command_offsets_.reserve(std::size_t{tick_count} + 1);

    for (std::uint32_t t = 0; t < tick_count; ++t) {
        std::uint32_t count = 0;
        if (!in.read(count) || count > in.remaining() / command_size) {
            return std::unexpected{RecordingError::truncated};
        }
        if (rec.commands_.size() + count > std::numeric_limits<std::uint32_t>::max()) {
            return std::unexpected{RecordingError::invalid_field};
        }
        for (std::uint32_t c = 0; c < count; ++c) {
            Command cmd;
            if (!in.read(cmd.body) || !read_vec3(in, cmd.impulse)) {
                return std::unexpected{RecordingError::truncated};
            }
            if (cmd.body >= s.body_count) {
                return std::unexpected{RecordingError::invalid_field};
            }
            rec.commands_.push_back(cmd);
        }
        std::uint64_t hash = 0;
        if (!in.read(hash)) {
            return std::unexpected{RecordingError::truncated};
        }
        rec.command_offsets_.push_back(static_cast<std::uint32_t>(rec.commands_.size()));
        rec.hashes_.push_back(hash);
    }

    if (in.remaining() != 0) {
        return std::unexpected{RecordingError::trailing_data};
    }
    return rec;
}

std::expected<void, RecordingError> save(const Recording& recording, const std::filesystem::path& path) {
    const std::vector<std::byte> bytes = recording.serialize();
    std::ofstream file{path, std::ios::binary | std::ios::trunc};
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file) {
        return std::unexpected{RecordingError::io_error};
    }
    return {};
}

std::expected<Recording, RecordingError> load(const std::filesystem::path& path) {
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    if (!file) {
        return std::unexpected{RecordingError::io_error};
    }
    const std::streamoff size = file.tellg();
    if (size < 0) {
        return std::unexpected{RecordingError::io_error};
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        return std::unexpected{RecordingError::io_error};
    }
    return Recording::deserialize(bytes);
}

} // namespace idem
