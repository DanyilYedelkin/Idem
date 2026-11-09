#include "options.hpp"

#include <algorithm>

namespace idem::cli {

std::expected<Options, std::string> Options::parse(std::span<const std::string_view> args) {
    Options options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        if (!arg.starts_with("--")) {
            options.positional_.push_back(arg);
            continue;
        }
        const std::string_view name = arg.substr(2);
        if (name.empty()) {
            return std::unexpected{std::string{"empty option name"}};
        }
        const bool has_value = i + 1 < args.size() && !args[i + 1].starts_with("--");
        options.flags_[name] = has_value ? args[++i] : std::string_view{};
    }
    return options;
}

std::expected<void, std::string> Options::allow_only(std::initializer_list<std::string_view> names) const {
    for (const auto& [name, value] : flags_) {
        if (std::ranges::find(names, name) == names.end()) {
            return std::unexpected{"unknown option --" + std::string{name}};
        }
    }
    return {};
}

std::string_view Options::get(std::string_view name, std::string_view fallback) const {
    const auto it = flags_.find(name);
    return it == flags_.end() ? fallback : it->second;
}

} // namespace idem::cli
