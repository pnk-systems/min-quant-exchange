#pragma once
#include <string>
#include <unordered_map>

#include "miniq/trade.hpp"
#include "miniq/types.hpp"

namespace miniq {

// PositionService — SRP: tracks net position per symbol for the single
// implicit trading account this project models, nothing else. It does
// not know about orders, books, or risk; it only understands "apply this
// signed quantity change." ExchangeService adjusts it after each fill;
// RiskEngine reads from it (via ExchangeService) before accepting new
// orders. See docs/DESIGN_DECISIONS.md for the single-account
// simplification and what a multi-account version would need.
class PositionService {
public:
    void adjust(const Symbol& symbol, Side side, Quantity quantity);

    [[nodiscard]] Quantity positionFor(const Symbol& symbol) const;

private:
    std::unordered_map<std::string, Quantity> positions_;
};

} // namespace miniq
