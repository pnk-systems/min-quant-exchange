#include "miniq/strategy.hpp"

namespace miniq {

std::optional<OrderRequest> PassiveJoinStrategy::onMarketData(const MarketSnapshot& snapshot) {
    if (!snapshot.bestAsk) {
        return std::nullopt; // nothing to join against
    }
    OrderRequest request{
        OrderId{nextId_++},
        snapshot.symbol,
        Side::Buy,
        OrderType::Limit,
        quantity_,
        *snapshot.bestAsk - 1,
    };
    return request;
}

} // namespace miniq
