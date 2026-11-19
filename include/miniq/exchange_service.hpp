#pragma once
#include <memory>
#include <unordered_set>
#include <vector>

#include "miniq/clock.hpp"
#include "miniq/event_bus.hpp"
#include "miniq/matching_engine.hpp"
#include "miniq/position_service.hpp"
#include "miniq/risk.hpp"

namespace miniq {

// Result of submitting an order request: either accepted (with any
// resulting trades) or rejected with a reason. An explicit result type,
// not an exception — domain rejection is a routine, expected outcome
// (see docs/DESIGN_DECISIONS.md #8).
struct SubmitResult {
    bool accepted{false};
    RejectReason rejectReason{RejectReason::None};
    std::vector<Trade> trades;
};

// ExchangeService — the orchestration boundary (DIP): depends on
// MatchingEngine, RiskEngine, PositionService, EventBus and IClock only
// through their own public interfaces/types, and receives them via
// constructor injection rather than constructing them internally. This is
// what makes it possible to substitute a ManualClock or a stub RiskEngine
// in tests without touching ExchangeService itself.
//
// Responsibilities, in order, for submit():
//   1. validate the request (quantity, price presence);
//   2. reject duplicate order IDs;
//   3. run risk checks;
//   4. hand accepted orders to MatchingEngine;
//   5. update positions from resulting trades;
//   6. publish events.
class ExchangeService {
public:
    ExchangeService(MatchingEngine& matchingEngine, RiskEngine& riskEngine,
                     PositionService& positionService, EventBus& eventBus, IClock& clock)
        : matchingEngine_(matchingEngine),
          riskEngine_(riskEngine),
          positionService_(positionService),
          eventBus_(eventBus),
          clock_(clock) {}

    [[nodiscard]] SubmitResult submit(const OrderRequest& request);
    [[nodiscard]] CancelResult cancel(const Symbol& symbol, OrderId id);

    [[nodiscard]] std::optional<PriceTicks> bestBid(const Symbol& symbol) const {
        return matchingEngine_.bestBid(symbol);
    }
    [[nodiscard]] std::optional<PriceTicks> bestAsk(const Symbol& symbol) const {
        return matchingEngine_.bestAsk(symbol);
    }
    [[nodiscard]] Quantity positionFor(const Symbol& symbol) const {
        return positionService_.positionFor(symbol);
    }

private:
    [[nodiscard]] std::optional<RejectReason> validate(const OrderRequest& request) const;

    MatchingEngine& matchingEngine_;
    RiskEngine& riskEngine_;
    PositionService& positionService_;
    EventBus& eventBus_;
    IClock& clock_;

    // Duplicate-ID detection needs a global view across all symbols'
    // books, which MatchingEngine does not expose — kept here instead of
    // pushed down into every OrderBook.
    std::unordered_set<OrderId::ValueType> knownOrderIds_;
};

} // namespace miniq
