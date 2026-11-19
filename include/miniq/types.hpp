#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

#include "miniq/strong_id.hpp"

namespace miniq {

// ---------------------------------------------------------------------------
// Value/domain types. These are small, immutable-by-convention value objects.
// Keeping them free of behavior beyond validation keeps the "entity" classes
// (Order, Trade) easy to reason about — a Law-of-Demeter-friendly design.
// ---------------------------------------------------------------------------

// Tag types only exist to make StrongId<OrderIdTag> != StrongId<TradeIdTag>
// at compile time. They carry no data.
struct OrderIdTag {};
struct TradeIdTag {};

using OrderId = StrongId<OrderIdTag>;
using TradeId = StrongId<TradeIdTag>;

// Symbol is a validated value object, not a full security master entry.
// WHY explicit: a bare `std::string` would let "" or "AAPL " (trailing
// space) slip through as a distinct, silently-wrong symbol.
class Symbol {
public:
    explicit Symbol(std::string code) : code_(std::move(code)) {
        if (code_.empty() || code_.size() > 16) {
            throw std::invalid_argument("Symbol must be 1-16 characters");
        }
    }

    [[nodiscard]] const std::string& code() const noexcept { return code_; }

    friend bool operator==(const Symbol&, const Symbol&) = default;

private:
    std::string code_;
};

struct SymbolHash {
    size_t operator()(const Symbol& symbol) const noexcept {
        return std::hash<std::string>{}(symbol.code());
    }
};

// Price is expressed in integer ticks, never floating point. See
// docs/DESIGN_DECISIONS.md #1 for the full rationale: exact ordering/
// equality and a deterministic book without binary floating-point surprises.
using PriceTicks = std::int64_t;

// Quantity is a positive integer number of units/shares/contracts.
using Quantity = std::int64_t;

enum class Side { Buy, Sell };

enum class OrderType { Limit, Market };

// Order lifecycle. REJECTED orders never become live book orders — they are
// a terminal outcome of validation/risk, not a state a resting Order visits.
enum class OrderStatus { New, PartiallyFilled, Filled, Cancelled, Rejected };

enum class RejectReason {
    None,
    InvalidQuantity,
    InvalidPrice,
    UnknownSymbol,
    DuplicateOrderId,
    MaxQuantityExceeded,
    MaxNotionalExceeded,
    MaxPositionExceeded,
};

enum class CancelResult { Cancelled, UnknownOrder, NotLive };

[[nodiscard]] constexpr const char* toString(Side side) noexcept {
    switch (side) {
        case Side::Buy: return "BUY";
        case Side::Sell: return "SELL";
    }
    return "?";
}

[[nodiscard]] constexpr const char* toString(OrderType type) noexcept {
    switch (type) {
        case OrderType::Limit: return "LIMIT";
        case OrderType::Market: return "MARKET";
    }
    return "?";
}

[[nodiscard]] constexpr const char* toString(OrderStatus status) noexcept {
    switch (status) {
        case OrderStatus::New: return "NEW";
        case OrderStatus::PartiallyFilled: return "PARTIALLY_FILLED";
        case OrderStatus::Filled: return "FILLED";
        case OrderStatus::Cancelled: return "CANCELLED";
        case OrderStatus::Rejected: return "REJECTED";
    }
    return "?";
}

[[nodiscard]] constexpr const char* toString(RejectReason reason) noexcept {
    switch (reason) {
        case RejectReason::None: return "NONE";
        case RejectReason::InvalidQuantity: return "INVALID_QUANTITY";
        case RejectReason::InvalidPrice: return "INVALID_PRICE";
        case RejectReason::UnknownSymbol: return "UNKNOWN_SYMBOL";
        case RejectReason::DuplicateOrderId: return "DUPLICATE_ORDER_ID";
        case RejectReason::MaxQuantityExceeded: return "MAX_QUANTITY_EXCEEDED";
        case RejectReason::MaxNotionalExceeded: return "MAX_NOTIONAL_EXCEEDED";
        case RejectReason::MaxPositionExceeded: return "MAX_POSITION_EXCEEDED";
    }
    return "?";
}

} // namespace miniq
