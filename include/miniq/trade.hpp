#pragma once
#include "miniq/types.hpp"

namespace miniq {

// Trade — an immutable record of a single match between an incoming order
// and one resting order. Execution price rule: the RESTING order's price
// (the order that was already on the book) is used. This is a simple,
// explicit, commonly-used convention ("the resting side sets the price")
// that avoids the ambiguity of averaging or picking the aggressor's price.
struct Trade {
    TradeId id;
    Symbol symbol;
    OrderId aggressorOrderId;   // the incoming order that crossed the book
    OrderId restingOrderId;     // the order that was already resting
    Side aggressorSide;
    PriceTicks priceTicks{0};   // always the resting order's price
    Quantity quantity{0};
    std::uint64_t sequence{0};
};

} // namespace miniq
