#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace idem::detail {

template <class T>
concept Scalar = std::is_integral_v<T> || std::is_floating_point_v<T>;

template <class T>
using UnsignedOfSize =
    std::conditional_t<sizeof(T) == 1, std::uint8_t,
                       std::conditional_t<sizeof(T) == 2, std::uint16_t,
                                          std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>>>;

// The on-disk format is little-endian regardless of the host.
template <Scalar T>
[[nodiscard]] constexpr auto to_little_endian(T value) noexcept {
    auto bits = std::bit_cast<UnsignedOfSize<T>>(value);
    if constexpr (std::endian::native == std::endian::big) {
        bits = std::byteswap(bits);
    }
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(bits);
}

template <Scalar T>
[[nodiscard]] constexpr T from_little_endian(std::span<const std::byte, sizeof(T)> bytes) noexcept {
    std::array<std::byte, sizeof(T)> copy{};
    std::ranges::copy(bytes, copy.begin());
    auto bits = std::bit_cast<UnsignedOfSize<T>>(copy);
    if constexpr (std::endian::native == std::endian::big) {
        bits = std::byteswap(bits);
    }
    return std::bit_cast<T>(bits);
}

class ByteWriter {
public:
    template <Scalar T>
    void write(T value) {
        const auto bytes = to_little_endian(value);
        buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    }

    void write_bytes(std::span<const std::byte> bytes) {
        buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    }

    [[nodiscard]] std::vector<std::byte> take() && noexcept { return std::move(buffer_); }

private:
    std::vector<std::byte> buffer_;
};

class ByteReader {
public:
    explicit ByteReader(std::span<const std::byte> bytes) noexcept : bytes_{bytes} {}

    template <Scalar T>
    [[nodiscard]] bool read(T& out) noexcept {
        if (remaining() < sizeof(T)) {
            return false;
        }
        out = from_little_endian<T>(bytes_.subspan(offset_).template first<sizeof(T)>());
        offset_ += sizeof(T);
        return true;
    }

    [[nodiscard]] bool read_bytes(std::span<std::byte> out) noexcept {
        if (remaining() < out.size()) {
            return false;
        }
        std::memcpy(out.data(), bytes_.data() + offset_, out.size());
        offset_ += out.size();
        return true;
    }

    [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - offset_; }

private:
    std::span<const std::byte> bytes_;
    std::size_t offset_ = 0;
};

} // namespace idem::detail
