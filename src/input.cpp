#include <idem/input.hpp>

namespace idem {

namespace {
constexpr std::uint32_t max_commands_per_tick = 4;
}

InputScript::InputScript(std::uint64_t seed, std::uint32_t body_count, float strength)
    : rng_{seed}, body_count_{body_count}, strength_{strength} {
    buffer_.reserve(max_commands_per_tick);
}

std::span<const Command> InputScript::next_tick() {
    buffer_.clear();
    if (body_count_ == 0) {
        return {};
    }

    const std::uint32_t count = rng_.below(max_commands_per_tick + 1);
    for (std::uint32_t i = 0; i < count; ++i) {
        Command cmd;
        cmd.body = rng_.below(body_count_);
        const float s = strength_;
        cmd.impulse = {rng_.uniform(-s, s), rng_.uniform(0.0f, 2.0f * s), rng_.uniform(-s, s)};
        buffer_.push_back(cmd);
    }
    return buffer_;
}

} // namespace idem
