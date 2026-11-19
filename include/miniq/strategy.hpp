#pragma once
#include <optional>

#include "miniq/order.hpp"

namespace miniq {

// Snapshot of top-of-book state a strategy can react to. Deliberately
// minimal (ISP): a strategy sees only what it needs to decide, not the
// whole OrderBook internals.
struct MarketSnapshot {
    Symbol symbol;
    std::optional<PriceTicks> bestBid;
    std::optional<PriceTicks> bestAsk;
};

// ITradingStrategy — Strategy pattern (OCP): new strategies are added by
// implementing this interface, never by editing existing strategy code or
// ExchangeService. Kept intentionally small for this educational project;
// a production strategy interface would also receive fills/position.
class ITradingStrategy {
public:
    virtual ~ITradingStrategy() = default;

    // Returns an order request to submit, or std::nullopt to do nothing
    // this tick. std::optional communicates "no action" as a normal,
    // expected outcome rather than a sentinel value.
    [[nodiscard]] virtual std::optional<OrderRequest> onMarketData(
        const MarketSnapshot& snapshot) = 0;
};

// A trivial example strategy: place a passive buy one tick below the best
// ask (or do nothing if there is no ask yet). Exists to demonstrate the
// Strategy interface end-to-end in the CLI demo, not as a real alpha model.
class PassiveJoinStrategy final : public ITradingStrategy {
public:
    PassiveJoinStrategy(OrderId::ValueType startingId, Quantity quantity)
        : nextId_(startingId), quantity_(quantity) {}

    [[nodiscard]] std::optional<OrderRequest> onMarketData(
        const MarketSnapshot& snapshot) override;

private:
    OrderId::ValueType nextId_;
    Quantity quantity_;
};

} // namespace miniq
