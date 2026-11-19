#include <catch2/catch_test_macros.hpp>
#include <limits>

#include "miniq/risk.hpp"

using namespace miniq;

namespace {
OrderRequest limitRequest(Quantity qty, PriceTicks price, Side side = Side::Buy) {
    return OrderRequest{OrderId{1}, Symbol{"AAPL"}, side, OrderType::Limit, qty, price};
}
} // namespace

TEST_CASE("max-quantity risk rejection", "[risk]") {
    RiskEngine engine;
    engine.addRule(std::make_unique<MaxQuantityRule>(100));

    auto tooLarge = limitRequest(101, 20'000);
    REQUIRE(engine.evaluate(tooLarge, 0) == std::optional<RejectReason>{RejectReason::MaxQuantityExceeded});

    auto ok = limitRequest(100, 20'000);
    REQUIRE(engine.evaluate(ok, 0) == std::nullopt);
}

TEST_CASE("max-notional risk rejection", "[risk]") {
    RiskEngine engine;
    engine.addRule(std::make_unique<MaxNotionalRule>(1'000'000));

    auto tooExpensive = limitRequest(100, 20'000); // notional = 2,000,000
    REQUIRE(engine.evaluate(tooExpensive, 0) ==
            std::optional<RejectReason>{RejectReason::MaxNotionalExceeded});

    auto ok = limitRequest(10, 20'000); // notional = 200,000
    REQUIRE(engine.evaluate(ok, 0) == std::nullopt);
}

TEST_CASE("position-limit rejection", "[risk]") {
    RiskEngine engine;
    engine.addRule(std::make_unique<MaxPositionRule>(500));

    auto request = limitRequest(600, 20'000, Side::Buy);
    REQUIRE(engine.evaluate(request, 0) ==
            std::optional<RejectReason>{RejectReason::MaxPositionExceeded});

    // Already long 400; buying 200 more would project to 600 -> exceeds 500.
    auto incremental = limitRequest(200, 20'000, Side::Buy);
    REQUIRE(engine.evaluate(incremental, 400) ==
            std::optional<RejectReason>{RejectReason::MaxPositionExceeded});

    // Selling to reduce a long position should not be rejected by the limit.
    auto reducing = limitRequest(200, 20'000, Side::Sell);
    REQUIRE(engine.evaluate(reducing, 400) == std::nullopt);
}

TEST_CASE("arithmetic boundary/overflow protection in notional check", "[risk]") {
    RiskEngine engine;
    engine.addRule(std::make_unique<MaxNotionalRule>(1'000'000));

    // Deliberately large price*quantity that would overflow a naive
    // int64 multiplication; MaxNotionalRule must detect this and reject
    // rather than wrap around to a small/negative notional.
    auto overflowing = limitRequest(std::numeric_limits<std::int64_t>::max() / 2, 4, Side::Buy);
    REQUIRE(engine.evaluate(overflowing, 0) ==
            std::optional<RejectReason>{RejectReason::MaxNotionalExceeded});
}

TEST_CASE("market orders are exempt from the notional rule", "[risk]") {
    RiskEngine engine;
    engine.addRule(std::make_unique<MaxNotionalRule>(100));

    OrderRequest marketRequest{OrderId{1}, Symbol{"AAPL"}, Side::Buy, OrderType::Market, 10'000,
                                std::nullopt};
    // Documented policy: notional is unknown pre-match for a market order,
    // so MaxNotionalRule does not evaluate it (see risk.hpp).
    REQUIRE(engine.evaluate(marketRequest, 0) == std::nullopt);
}

TEST_CASE("multiple rules evaluate in order and the first violation wins", "[risk]") {
    RiskEngine engine;
    engine.addRule(std::make_unique<MaxQuantityRule>(50));
    engine.addRule(std::make_unique<MaxNotionalRule>(1));

    auto request = limitRequest(100, 20'000); // violates both rules
    REQUIRE(engine.evaluate(request, 0) ==
            std::optional<RejectReason>{RejectReason::MaxQuantityExceeded}); // first rule added wins
}
