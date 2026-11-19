#pragma once
#include <optional>

#include "miniq/types.hpp"

namespace miniq {

// OrderRequest — what a client asks for. Distinct from Order (what the
// exchange tracks) because a request may be rejected before ever becoming
// a live, stateful book entity. Keeping them separate avoids an Order
// object existing in a half-valid "not yet accepted" state.
struct OrderRequest {
    OrderId id;
    Symbol symbol;
    Side side{Side::Buy};
    OrderType type{OrderType::Limit};
    Quantity quantity{0};
    // Required for Limit orders, ignored for Market orders (policy: any
    // price supplied on a Market request is simply not looked at).
    std::optional<PriceTicks> limitPrice;
};

// Order — the exchange's live representation of an accepted request.
//
// Encapsulation: remaining quantity and status are only mutable through
// fill()/cancel(), which enforce the invariant
//     0 <= remainingQuantity <= originalQuantity
// and reject illegal state transitions (e.g. filling a cancelled order).
// This is why Order is a class with private data rather than a public
// struct/DTO: it protects an invariant, not just groups fields.
class Order {
public:
    Order(OrderId id, Symbol symbol, Side side, OrderType type, Quantity quantity,
          std::optional<PriceTicks> limitPrice, std::uint64_t sequence)
        : id_(id),
          symbol_(std::move(symbol)),
          side_(side),
          type_(type),
          originalQuantity_(quantity),
          remainingQuantity_(quantity),
          limitPrice_(limitPrice),
          sequence_(sequence),
          status_(OrderStatus::New) {}

    [[nodiscard]] OrderId id() const noexcept { return id_; }
    [[nodiscard]] const Symbol& symbol() const noexcept { return symbol_; }
    [[nodiscard]] Side side() const noexcept { return side_; }
    [[nodiscard]] OrderType type() const noexcept { return type_; }
    [[nodiscard]] Quantity originalQuantity() const noexcept { return originalQuantity_; }
    [[nodiscard]] Quantity remainingQuantity() const noexcept { return remainingQuantity_; }
    [[nodiscard]] std::optional<PriceTicks> limitPrice() const noexcept { return limitPrice_; }
    // Sequence number provides a deterministic time-priority tie-breaker
    // within a price level, independent of wall-clock resolution.
    [[nodiscard]] std::uint64_t sequence() const noexcept { return sequence_; }
    [[nodiscard]] OrderStatus status() const noexcept { return status_; }
    [[nodiscard]] bool isLive() const noexcept {
        return status_ == OrderStatus::New || status_ == OrderStatus::PartiallyFilled;
    }

    // Applies an execution of `filledQuantity` units. Caller (OrderBook)
    // guarantees filledQuantity <= remainingQuantity_; this is enforced
    // defensively regardless, since a broken invariant here corrupts the
    // book silently otherwise.
    void fill(Quantity filledQuantity) {
        if (!isLive()) {
            throw std::logic_error("Cannot fill a non-live order");
        }
        if (filledQuantity <= 0 || filledQuantity > remainingQuantity_) {
            throw std::logic_error("Invalid fill quantity");
        }
        remainingQuantity_ -= filledQuantity;
        status_ = (remainingQuantity_ == 0) ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
    }

    // Returns false if the order was not live (already filled/cancelled) —
    // an explicit, non-throwing failure signal for a routine business
    // outcome (see docs/DESIGN_DECISIONS.md #8: exceptions vs domain results).
    [[nodiscard]] bool cancel() noexcept {
        if (!isLive()) {
            return false;
        }
        status_ = OrderStatus::Cancelled;
        return true;
    }

private:
    OrderId id_;
    Symbol symbol_;
    Side side_;
    OrderType type_;
    Quantity originalQuantity_;
    Quantity remainingQuantity_;
    std::optional<PriceTicks> limitPrice_;
    std::uint64_t sequence_;
    OrderStatus status_;
};

} // namespace miniq
