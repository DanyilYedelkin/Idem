#pragma once

#include <charconv>
#include <concepts>
#include <expected>
#include <initializer_list>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace idem::cli {

// Minimal "--name value" parser; pulling in a dependency for four subcommands isn't worth it.
class Options {
public:
    [[nodiscard]] static std::expected<Options, std::string> parse(std::span<const std::string_view> args);

    [[nodiscard]] const std::vector<std::string_view>& positional() const noexcept { return positional_; }
    [[nodiscard]] bool has(std::string_view name) const { return flags_.contains(name); }
    [[nodiscard]] std::expected<void, std::string>
    allow_only(std::initializer_list<std::string_view> names) const;

    [[nodiscard]] std::string_view get(std::string_view name, std::string_view fallback) const;

    template <std::integral T>
    [[nodiscard]] std::expected<T, std::string> get(std::string_view name, T fallback) const {
        const auto it = flags_.find(name);
        if (it == flags_.end()) {
            return fallback;
        }
        const std::string_view text = it->second;
        T value{};
        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc{} || end != text.data() + text.size()) {
            return std::unexpected{"invalid value for --" + std::string{name} + ": '" + std::string{text} +
                                   "'"};
        }
        return value;
    }

private:
    std::vector<std::string_view> positional_;
    std::map<std::string_view, std::string_view, std::less<>> flags_;
};

} // namespace idem::cli
