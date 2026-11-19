#include "miniq/matching_engine.hpp"

namespace miniq {

OrderBook& MatchingEngine::bookFor(const Symbol& symbol) {
    auto it = books_.find(symbol.code());
    if (it == books_.end()) {
        it = books_.emplace(symbol.code(), OrderBook{symbol}).first;
    }
    return it->second;
}

const OrderBook* MatchingEngine::findBook(const Symbol& symbol) const {
    auto it = books_.find(symbol.code());
    return it == books_.end() ? nullptr : &it->second;
}

void MatchingEngine::submit(Order order, std::vector<Trade>& outTrades, IClock& clock) {
    bookFor(order.symbol()).submit(std::move(order), outTrades, clock);
}

CancelResult MatchingEngine::cancel(const Symbol& symbol, OrderId id) {
    auto it = books_.find(symbol.code());
    if (it == books_.end()) {
        return CancelResult::UnknownOrder;
    }
    return it->second.cancel(id);
}

std::optional<PriceTicks> MatchingEngine::bestBid(const Symbol& symbol) const {
    const auto* book = findBook(symbol);
    return book ? book->bestBid() : std::nullopt;
}

std::optional<PriceTicks> MatchingEngine::bestAsk(const Symbol& symbol) const {
    const auto* book = findBook(symbol);
    return book ? book->bestAsk() : std::nullopt;
}

} // namespace miniq
