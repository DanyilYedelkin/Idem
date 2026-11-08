#pragma once

#include <idem/math.hpp>
#include <idem/rng.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace idem {

// An external input applied at the start of a tick. In a real exercise these
// would come from operators or AI; here they are the only thing a replay stores
// besides the initial conditions.
struct Command {
    std::uint32_t body = 0;
    Vec3 impulse;

    friend constexpr bool operator==(const Command&, const Command&) noexcept = default;
};

// Seeded stand-in for a live input stream.
class InputScript {
public:
    InputScript(std::uint64_t seed, std::uint32_t body_count, float strength = 0.25f);

    [[nodiscard]] std::span<const Command> next_tick();

private:
    Xoshiro256 rng_;
    std::uint32_t body_count_;
    float strength_;
    std::vector<Command> buffer_;
};

} // namespace idem
