#include "miniq/position_service.hpp"

namespace miniq {

void PositionService::adjust(const Symbol& symbol, Side side, Quantity quantity) {
    const Quantity signedQty = (side == Side::Buy) ? quantity : -quantity;
    positions_[symbol.code()] += signedQty;
}

Quantity PositionService::positionFor(const Symbol& symbol) const {
    auto it = positions_.find(symbol.code());
    return it == positions_.end() ? 0 : it->second;
}

} // namespace miniq
