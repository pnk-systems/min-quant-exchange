#pragma once
#include <unordered_map>

#include "miniq/clock.hpp"
#include "miniq/order_book.hpp"

namespace miniq {

// MatchingEngine — owns and routes to one OrderBook per symbol. It does
// not know about risk or events; it only understands "route this accepted
// order to its book and report what happened." SRP: ExchangeService
// orchestrates, RiskEngine gatekeeps, MatchingEngine matches.
class MatchingEngine {
public:
    // Submits an already-accepted order to its symbol's book, creating the
    // book lazily on first use for that symbol (explicit creation avoids
    // silently growing an unbounded map of dead symbols).
    void submit(Order order, std::vector<Trade>& outTrades, IClock& clock);

    [[nodiscard]] CancelResult cancel(const Symbol& symbol, OrderId id);

    [[nodiscard]] std::optional<PriceTicks> bestBid(const Symbol& symbol) const;
    [[nodiscard]] std::optional<PriceTicks> bestAsk(const Symbol& symbol) const;

private:
    [[nodiscard]] OrderBook& bookFor(const Symbol& symbol);
    [[nodiscard]] const OrderBook* findBook(const Symbol& symbol) const;

    // unordered_map<Symbol, OrderBook>: average O(1) routing by symbol.
    // OrderBook is move-only (see order_book.hpp), so books live directly
    // in the map rather than behind unique_ptr — one fewer indirection/
    // allocation per symbol, and ownership is unambiguous (the map owns
    // them outright).
    std::unordered_map<std::string, OrderBook> books_;
};

} // namespace miniq
