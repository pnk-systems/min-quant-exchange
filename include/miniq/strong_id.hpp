#pragma once
#include <compare>
#include <cstdint>
#include <functional>

namespace miniq {

// StrongId<Tag> — a zero-cost wrapper around an integer that prevents mixing
// unrelated ID spaces (e.g. accidentally comparing an OrderId to a TradeId).
// WHY: `using OrderId = std::uint64_t;` gives no type safety since any
// integer implicitly converts. A distinct tag type per ID makes such misuse
// a compile error instead of a runtime bug. This is the project's one
// class template; it demonstrates function/class template basics without
// drifting into template metaprogramming.
template <typename Tag>
class StrongId {
public:
    using ValueType = std::uint64_t;

    StrongId() = default;
    explicit constexpr StrongId(ValueType value) noexcept : value_(value) {}

    [[nodiscard]] constexpr ValueType value() const noexcept { return value_; }

    // Rule of Zero: only a scalar member, so defaulted comparisons suffice.
    friend constexpr auto operator<=>(const StrongId&, const StrongId&) = default;

private:
    ValueType value_{0};
};

} // namespace miniq

// Enable use as a key in std::unordered_map without writing a manual hash
// specialization at every call site.
namespace std {
template <typename Tag>
struct hash<miniq::StrongId<Tag>> {
    size_t operator()(const miniq::StrongId<Tag>& id) const noexcept {
        return std::hash<typename miniq::StrongId<Tag>::ValueType>{}(id.value());
    }
};
} // namespace std
