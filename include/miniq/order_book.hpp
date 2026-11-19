#pragma once
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "miniq/clock.hpp"
#include "miniq/order.hpp"
#include "miniq/trade.hpp"

namespace miniq {

// OrderBook — owns all live resting orders for exactly ONE symbol and
// enforces price-time priority. This is the most invariant-heavy class in
// the project, so its internals are entirely private; callers only ever
// submit/cancel/query, never touch price levels directly (information
// hiding + encapsulation).
//
// Container choices (see docs/DESIGN_DECISIONS.md #2 for full trade-offs):
//   bids_: map<PriceTicks, PriceLevel, greater<>>  — first entry = best (highest) bid
//   asks_: map<PriceTicks, PriceLevel, less<>>     — first entry = best (lowest) ask
//   each PriceLevel::orders is a list<Order> for O(1) erase-with-iterator
//     and natural FIFO (push_back + pop_front).
//   locators_: unordered_map<OrderId, Locator> for O(1) average cancel lookup.
class OrderBook {
public:
    explicit OrderBook(Symbol symbol) : symbol_(std::move(symbol)) {}

    // Not copyable: a book's locator iterators point into its own list
    // nodes, so a shallow copy would alias state. Not needed anyway —
    // MatchingEngine owns exactly one instance per symbol.
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = default;
    OrderBook& operator=(OrderBook&&) = default;

    [[nodiscard]] const Symbol& symbol() const noexcept { return symbol_; }

    // Attempts to match `incoming` against the opposite side, then (for
    // LIMIT orders only) rests any unfilled remainder. Appends every Trade
    // produced to `outTrades`. Returns nothing — check incoming's own
    // status/remainingQuantity after the call, or read outTrades.
    //
    // Complexity: O(k log L) where k = number of price levels crossed and
    // L = number of distinct price levels on the contra side, plus O(1)
    // per matched resting order for the FIFO pop.
    void submit(Order incoming, std::vector<Trade>& outTrades, IClock& clock);

    [[nodiscard]] CancelResult cancel(OrderId id);

    [[nodiscard]] std::optional<PriceTicks> bestBid() const noexcept;
    [[nodiscard]] std::optional<PriceTicks> bestAsk() const noexcept;

    // For tests/inspection only — returns remaining quantity of a live
    // order, if it is currently resting on the book.
    [[nodiscard]] std::optional<Quantity> liveQuantity(OrderId id) const;

private:
    struct PriceLevel {
        std::list<Order> orders; // front() = oldest = highest time priority
    };

    // Locates a resting order without scanning every price level: which
    // side, which price bucket, and the exact list iterator to erase.
    struct Locator {
        Side side;
        PriceTicks price;
        std::list<Order>::iterator it;
    };

    template <typename Book>
    void matchAgainst(Order& incoming, Book& contraBook, Side contraSide,
                       std::vector<Trade>& outTrades, IClock& clock);

    void rest(Order order);
    void eraseLocator(OrderId id);

    Symbol symbol_;
    std::map<PriceTicks, PriceLevel, std::greater<>> bids_;
    std::map<PriceTicks, PriceLevel, std::less<>> asks_;
    std::unordered_map<OrderId, Locator> locators_;
};

} // namespace miniq
