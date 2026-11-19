#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <tuple>

#include "miniq/event_bus.hpp"
#include "miniq/exchange_service.hpp"
#include "miniq/matching_engine.hpp"
#include "miniq/position_service.hpp"
#include "miniq/risk.hpp"

using namespace miniq;

namespace {

struct Fixture {
    MatchingEngine matchingEngine;
    RiskEngine riskEngine;
    PositionService positionService;
    EventBus eventBus;
    ManualClock clock;
    ExchangeService exchange{matchingEngine, riskEngine, positionService, eventBus, clock};
};

// Records every event it receives, for assertions. Held via shared_ptr so
// EventBus's weak_ptr subscription mechanism can be exercised directly.
class RecordingListener final : public IEventListener {
public:
    void onEvent(const ExchangeEvent& event) override { events.push_back(event); }
    std::vector<ExchangeEvent> events;
};

} // namespace

TEST_CASE("duplicate ID rejection", "[exchange]") {
    Fixture fx;
    const Symbol aapl{"AAPL"};

    auto first = fx.exchange.submit({OrderId{1}, aapl, Side::Buy, OrderType::Limit, 10, 20'000});
    REQUIRE(first.accepted);

    auto duplicate =
        fx.exchange.submit({OrderId{1}, aapl, Side::Buy, OrderType::Limit, 10, 20'000});
    REQUIRE_FALSE(duplicate.accepted);
    REQUIRE(duplicate.rejectReason == RejectReason::DuplicateOrderId);
}

TEST_CASE("invalid quantity rejection", "[exchange]") {
    Fixture fx;
    const Symbol aapl{"AAPL"};

    auto result = fx.exchange.submit({OrderId{1}, aapl, Side::Buy, OrderType::Limit, 0, 20'000});
    REQUIRE_FALSE(result.accepted);
    REQUIRE(result.rejectReason == RejectReason::InvalidQuantity);
}

TEST_CASE("invalid limit price rejection", "[exchange]") {
    Fixture fx;
    const Symbol aapl{"AAPL"};

    auto missingPrice =
        fx.exchange.submit({OrderId{1}, aapl, Side::Buy, OrderType::Limit, 10, std::nullopt});
    REQUIRE_FALSE(missingPrice.accepted);
    REQUIRE(missingPrice.rejectReason == RejectReason::InvalidPrice);

    auto zeroPrice = fx.exchange.submit({OrderId{2}, aapl, Side::Buy, OrderType::Limit, 10, 0});
    REQUIRE_FALSE(zeroPrice.accepted);
    REQUIRE(zeroPrice.rejectReason == RejectReason::InvalidPrice);
}

TEST_CASE("position updated from fills", "[exchange]") {
    Fixture fx;
    const Symbol aapl{"AAPL"};

    std::ignore = fx.exchange.submit({OrderId{1}, aapl, Side::Sell, OrderType::Limit, 100, 20'000});
    REQUIRE(fx.exchange.positionFor(aapl) == 0); // resting order alone does not move position

    auto crossing = fx.exchange.submit({OrderId{2}, aapl, Side::Buy, OrderType::Limit, 40, 20'000});
    REQUIRE(crossing.trades.size() == 1);
    REQUIRE(fx.exchange.positionFor(aapl) == 40); // aggressor bought 40
}

TEST_CASE("event listener receives expected trade", "[exchange]") {
    Fixture fx;
    const Symbol aapl{"AAPL"};
    auto listener = std::make_shared<RecordingListener>();
    fx.eventBus.subscribe(listener);

    std::ignore = fx.exchange.submit({OrderId{1}, aapl, Side::Sell, OrderType::Limit, 100, 20'000});
    std::ignore = fx.exchange.submit({OrderId{2}, aapl, Side::Buy, OrderType::Limit, 100, 20'000});

    bool sawExpectedTrade = false;
    for (const auto& event : listener->events) {
        if (auto* tradeEvent = std::get_if<TradeExecuted>(&event)) {
            if (tradeEvent->trade.aggressorOrderId == OrderId{2} &&
                tradeEvent->trade.restingOrderId == OrderId{1} &&
                tradeEvent->trade.quantity == 100) {
                sawExpectedTrade = true;
            }
        }
    }
    REQUIRE(sawExpectedTrade);
}

TEST_CASE("expired weak listener does not crash event bus", "[exchange]") {
    EventBus bus;
    {
        auto listener = std::make_shared<RecordingListener>();
        bus.subscribe(listener);
        // listener destroyed at end of this scope; only a weak_ptr remains
        // in the bus.
    }
    // Publishing after the listener has expired must not dereference a
    // dangling pointer — EventBus::publish locks each weak_ptr and skips
    // any that failed to lock.
    REQUIRE_NOTHROW(bus.publish(OrderAccepted{OrderId{1}, Symbol{"AAPL"}}));
}

TEST_CASE("multi-symbol books are isolated", "[exchange]") {
    Fixture fx;
    const Symbol aapl{"AAPL"};
    const Symbol msft{"MSFT"};

    std::ignore = fx.exchange.submit({OrderId{1}, aapl, Side::Buy, OrderType::Limit, 10, 19'900});
    std::ignore = fx.exchange.submit({OrderId{2}, msft, Side::Buy, OrderType::Limit, 10, 30'000});

    REQUIRE(fx.exchange.bestBid(aapl) == std::optional<PriceTicks>{19'900});
    REQUIRE(fx.exchange.bestBid(msft) == std::optional<PriceTicks>{30'000});

    // Cancelling in one symbol's book must not affect the other.
    REQUIRE(fx.exchange.cancel(aapl, OrderId{1}) == CancelResult::Cancelled);
    REQUIRE(fx.exchange.bestBid(aapl) == std::nullopt);
    REQUIRE(fx.exchange.bestBid(msft) == std::optional<PriceTicks>{30'000});
}

TEST_CASE("full risk-integrated rejection flow", "[exchange]") {
    Fixture fx;
    fx.riskEngine.addRule(std::make_unique<MaxQuantityRule>(50));
    const Symbol aapl{"AAPL"};

    auto result = fx.exchange.submit({OrderId{1}, aapl, Side::Buy, OrderType::Limit, 100, 20'000});
    REQUIRE_FALSE(result.accepted);
    REQUIRE(result.rejectReason == RejectReason::MaxQuantityExceeded);
    REQUIRE(fx.exchange.bestBid(aapl) == std::nullopt); // rejected order never touches the book
}

TEST_CASE("cancel unknown and non-live orders via ExchangeService", "[exchange]") {
    Fixture fx;
    const Symbol aapl{"AAPL"};

    REQUIRE(fx.exchange.cancel(aapl, OrderId{999}) == CancelResult::UnknownOrder);

    std::ignore = fx.exchange.submit({OrderId{1}, aapl, Side::Sell, OrderType::Limit, 10, 20'000});
    std::ignore = fx.exchange.submit({OrderId{2}, aapl, Side::Buy, OrderType::Limit, 10, 20'000}); // fully fills order 1

    // Order 1 is now Filled, not live, and its locator has already been
    // removed by the matching engine -> reported as UnknownOrder, not
    // NotLive, since OrderBook does not retain filled orders.
    REQUIRE(fx.exchange.cancel(aapl, OrderId{1}) == CancelResult::UnknownOrder);
}
