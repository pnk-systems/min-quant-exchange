#pragma once
#include <optional>
#include <variant>

#include "miniq/trade.hpp"
#include "miniq/types.hpp"

namespace miniq {

// Closed set of exchange events. std::variant + std::visit models a
// closed sum type: the compiler flags a missing case in any visitor
// (via an exhaustive std::visit) at compile time, unlike a base-class-
// per-event-type hierarchy which fails silently (falls through to a
// default) if a new event type is added and a handler forgets it.

struct OrderAccepted {
    OrderId orderId;
    Symbol symbol;
};

struct OrderRejected {
    OrderId orderId;
    Symbol symbol;
    RejectReason reason;
};

struct OrderCancelled {
    OrderId orderId;
    Symbol symbol;
};

struct TradeExecuted {
    Trade trade;
};

struct BookUpdated {
    Symbol symbol;
    std::optional<PriceTicks> bestBid;
    std::optional<PriceTicks> bestAsk;
};

using ExchangeEvent =
    std::variant<OrderAccepted, OrderRejected, OrderCancelled, TradeExecuted, BookUpdated>;

} // namespace miniq
