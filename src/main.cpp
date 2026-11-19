// Demo CLI for MiniQuantExchange. Submits a small, deterministic sequence
// of orders using SystemClock, prints resulting events, then runs
// PassiveJoinStrategy for a couple of ticks to show the Strategy interface
// in action. This is a demonstration harness, not a production entrypoint.
#include <iostream>
#include <memory>
#include <tuple>

#include "miniq/event_bus.hpp"
#include "miniq/exchange_service.hpp"
#include "miniq/matching_engine.hpp"
#include "miniq/position_service.hpp"
#include "miniq/risk.hpp"
#include "miniq/strategy.hpp"

namespace {

using namespace miniq;

// A minimal IEventListener that prints every event to stdout via
// std::visit — an exhaustive visitor is a compile error if a new event
// variant is added and a case is forgotten here.
class ConsolePrinter final : public IEventListener {
public:
    void onEvent(const ExchangeEvent& event) override {
        std::visit(
            [](const auto& e) { print(e); },
            event);
    }

private:
    static void print(const OrderAccepted& e) {
        std::cout << "[ACCEPTED]  order=" << e.orderId.value() << " symbol=" << e.symbol.code()
                  << '\n';
    }
    static void print(const OrderRejected& e) {
        std::cout << "[REJECTED]  order=" << e.orderId.value() << " symbol=" << e.symbol.code()
                  << " reason=" << toString(e.reason) << '\n';
    }
    static void print(const OrderCancelled& e) {
        std::cout << "[CANCELLED] order=" << e.orderId.value() << " symbol=" << e.symbol.code()
                  << '\n';
    }
    static void print(const TradeExecuted& e) {
        std::cout << "[TRADE]     aggressor=" << e.trade.aggressorOrderId.value()
                  << " resting=" << e.trade.restingOrderId.value()
                  << " price=" << e.trade.priceTicks << " qty=" << e.trade.quantity << '\n';
    }
    static void print(const BookUpdated& e) {
        std::cout << "[BOOK]      symbol=" << e.symbol.code()
                  << " bid=" << (e.bestBid ? std::to_string(*e.bestBid) : "-")
                  << " ask=" << (e.bestAsk ? std::to_string(*e.bestAsk) : "-") << '\n';
    }
};

} // namespace

int main() {
    using namespace miniq;

    MatchingEngine matchingEngine;
    RiskEngine riskEngine;
    riskEngine.addRule(std::make_unique<MaxQuantityRule>(10'000));
    riskEngine.addRule(std::make_unique<MaxNotionalRule>(50'000'000));
    riskEngine.addRule(std::make_unique<MaxPositionRule>(5'000));

    PositionService positionService;
    EventBus eventBus;
    SystemClock clock;

    auto printer = std::make_shared<ConsolePrinter>();
    eventBus.subscribe(printer);

    ExchangeService exchange(matchingEngine, riskEngine, positionService, eventBus, clock);

    const Symbol aapl{"AAPL"};

    std::cout << "== Resting orders ==\n";
    std::ignore = exchange.submit({OrderId{1}, aapl, Side::Buy, OrderType::Limit, 100, 19'900});
    std::ignore = exchange.submit({OrderId{2}, aapl, Side::Sell, OrderType::Limit, 150, 20'100});

    std::cout << "\n== Crossing order ==\n";
    std::ignore = exchange.submit({OrderId{3}, aapl, Side::Buy, OrderType::Limit, 200, 20'100});

    std::cout << "\n== Cancel remaining resting order ==\n";
    std::ignore = exchange.cancel(aapl, OrderId{1});

    std::cout << "\n== Strategy demo (PassiveJoinStrategy) ==\n";
    std::ignore = exchange.submit({OrderId{4}, aapl, Side::Sell, OrderType::Limit, 100, 20'200});
    PassiveJoinStrategy strategy(100, 25);
    MarketSnapshot snapshot{aapl, exchange.bestBid(aapl), exchange.bestAsk(aapl)};
    if (auto request = strategy.onMarketData(snapshot)) {
        std::ignore = exchange.submit(*request);
    }

    std::cout << "\nFinal position(AAPL) = " << exchange.positionFor(aapl) << '\n';
    return 0;
}
