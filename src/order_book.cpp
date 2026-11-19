#include "miniq/order_book.hpp"

#include <algorithm>
#include <cassert>

namespace miniq {

namespace {
// A limit order crosses the book if its price is at least as aggressive
// as the best contra price. Market orders always cross (no price limit).
[[nodiscard]] bool crosses(const Order& incoming, PriceTicks contraBestPrice) {
    if (incoming.type() == OrderType::Market) {
        return true;
    }
    const PriceTicks limit = *incoming.limitPrice();
    return incoming.side() == Side::Buy ? limit >= contraBestPrice : limit <= contraBestPrice;
}
} // namespace

template <typename Book>
void OrderBook::matchAgainst(Order& incoming, Book& contraBook, Side contraSide,
                              std::vector<Trade>& outTrades, IClock& clock) {
    // WHILE, not a single pass: one incoming order can walk through
    // several resting orders (and price levels) until it is filled or the
    // contra side is exhausted.
    while (incoming.remainingQuantity() > 0 && !contraBook.empty()) {
        auto levelIt = contraBook.begin();
        const PriceTicks levelPrice = levelIt->first;
        if (!crosses(incoming, levelPrice)) {
            break; // best available contra price is no longer acceptable
        }

        PriceLevel& level = levelIt->second;
        assert(!level.orders.empty() && "price level must not be left empty");

        // FIFO: front() is always the oldest resting order at this price.
        auto restingIt = level.orders.begin();
        Order& resting = *restingIt;

        const Quantity fillQty = std::min(incoming.remainingQuantity(), resting.remainingQuantity());

        // Execution price rule: always the RESTING order's price.
        Trade trade{
            TradeId{clock.nextSequence()},
            symbol_,
            incoming.id(),
            resting.id(),
            incoming.side(),
            resting.limitPrice().value_or(levelPrice),
            fillQty,
            clock.nextSequence(),
        };
        outTrades.push_back(trade);

        incoming.fill(fillQty);
        resting.fill(fillQty);

        if (!resting.isLive()) {
            // Resting order fully filled: remove from FIFO list and drop
            // its cancel locator. erase() on a std::list only invalidates
            // the erased iterator, so this is safe mid-loop.
            eraseLocator(resting.id());
            level.orders.erase(restingIt);
            if (level.orders.empty()) {
                contraBook.erase(levelIt); // avoid leaving an empty level behind
            }
        }
        // else: resting order partially filled, stays at front() (still
        // oldest, still fully time-priority-correct — it does not move
        // to the back on a partial fill, which matches real price-time
        // priority semantics).
        (void)contraSide;
    }
}

void OrderBook::submit(Order incoming, std::vector<Trade>& outTrades, IClock& clock) {
    if (incoming.side() == Side::Buy) {
        matchAgainst(incoming, asks_, Side::Sell, outTrades, clock);
    } else {
        matchAgainst(incoming, bids_, Side::Buy, outTrades, clock);
    }

    if (incoming.remainingQuantity() > 0) {
        if (incoming.type() == OrderType::Limit) {
            rest(std::move(incoming));
        }
        // Market order remainder policy: NEVER rests. Any unfilled
        // quantity on a market order is simply dropped (documented, not
        // silently discarded — see docs/DESIGN_DECISIONS.md).
    }
}

void OrderBook::rest(Order order) {
    const OrderId id = order.id();
    const Side side = order.side();
    const PriceTicks price = *order.limitPrice();

    if (side == Side::Buy) {
        auto& level = bids_[price];
        level.orders.push_back(std::move(order));
        auto it = std::prev(level.orders.end());
        locators_.emplace(id, Locator{side, price, it});
    } else {
        auto& level = asks_[price];
        level.orders.push_back(std::move(order));
        auto it = std::prev(level.orders.end());
        locators_.emplace(id, Locator{side, price, it});
    }
}

void OrderBook::eraseLocator(OrderId id) { locators_.erase(id); }

CancelResult OrderBook::cancel(OrderId id) {
    auto found = locators_.find(id);
    if (found == locators_.end()) {
        return CancelResult::UnknownOrder;
    }

    const Locator locator = found->second; // copy: about to erase the map entry
    Order& order = *locator.it;
    if (!order.cancel()) {
        return CancelResult::NotLive; // defensive: should not happen if locators_ is kept in sync
    }

    if (locator.side == Side::Buy) {
        auto levelIt = bids_.find(locator.price);
        levelIt->second.orders.erase(locator.it);
        if (levelIt->second.orders.empty()) {
            bids_.erase(levelIt);
        }
    } else {
        auto levelIt = asks_.find(locator.price);
        levelIt->second.orders.erase(locator.it);
        if (levelIt->second.orders.empty()) {
            asks_.erase(levelIt);
        }
    }
    locators_.erase(found);
    return CancelResult::Cancelled;
}

std::optional<PriceTicks> OrderBook::bestBid() const noexcept {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<PriceTicks> OrderBook::bestAsk() const noexcept {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

std::optional<Quantity> OrderBook::liveQuantity(OrderId id) const {
    auto found = locators_.find(id);
    if (found == locators_.end()) return std::nullopt;
    return found->second.it->remainingQuantity();
}

} // namespace miniq
