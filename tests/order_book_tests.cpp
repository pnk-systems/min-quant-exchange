#include <catch2/catch_test_macros.hpp>

#include "miniq/clock.hpp"
#include "miniq/order_book.hpp"

using namespace miniq;

namespace {
Order makeOrder(std::uint64_t id, Side side, OrderType type, Quantity qty,
                 std::optional<PriceTicks> price, IClock& clock) {
    return Order{OrderId{id}, Symbol{"AAPL"}, side, type, qty, price, clock.nextSequence()};
}
} // namespace

TEST_CASE("limit buy rests when it does not cross", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Buy, OrderType::Limit, 100, 19'900, clock), trades, clock);

    REQUIRE(trades.empty());
    REQUIRE(book.bestBid() == std::optional<PriceTicks>{19'900});
    REQUIRE(book.bestAsk() == std::nullopt);
    REQUIRE(book.liveQuantity(OrderId{1}) == std::optional<Quantity>{100});
}

TEST_CASE("limit sell rests when it does not cross", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Sell, OrderType::Limit, 100, 20'100, clock), trades, clock);

    REQUIRE(trades.empty());
    REQUIRE(book.bestAsk() == std::optional<PriceTicks>{20'100});
    REQUIRE(book.bestBid() == std::nullopt);
}

TEST_CASE("buy crosses best ask", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Sell, OrderType::Limit, 100, 20'000, clock), trades, clock);
    book.submit(makeOrder(2, Side::Buy, OrderType::Limit, 100, 20'000, clock), trades, clock);

    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].priceTicks == 20'000); // resting (sell) order's price
    REQUIRE(trades[0].quantity == 100);
    REQUIRE(book.bestAsk() == std::nullopt);
    REQUIRE(book.bestBid() == std::nullopt);
}

TEST_CASE("sell crosses best bid", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Buy, OrderType::Limit, 100, 20'000, clock), trades, clock);
    book.submit(makeOrder(2, Side::Sell, OrderType::Limit, 100, 20'000, clock), trades, clock);

    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].priceTicks == 20'000); // resting (buy) order's price
    REQUIRE(book.bestBid() == std::nullopt);
}

TEST_CASE("FIFO at same price", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Sell, OrderType::Limit, 50, 20'000, clock), trades, clock);
    book.submit(makeOrder(2, Side::Sell, OrderType::Limit, 50, 20'000, clock), trades, clock);

    book.submit(makeOrder(3, Side::Buy, OrderType::Limit, 50, 20'000, clock), trades, clock);

    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].restingOrderId == OrderId{1}); // first-in, first matched
    REQUIRE(book.liveQuantity(OrderId{2}) == std::optional<Quantity>{50});
}

TEST_CASE("better price wins before worse price", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Sell, OrderType::Limit, 50, 20'100, clock), trades, clock);
    book.submit(makeOrder(2, Side::Sell, OrderType::Limit, 50, 20'000, clock), trades, clock); // better (lower) ask

    book.submit(makeOrder(3, Side::Buy, OrderType::Limit, 50, 20'200, clock), trades, clock);

    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].restingOrderId == OrderId{2}); // the better-priced ask matches first
}

TEST_CASE("partial fill", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Sell, OrderType::Limit, 100, 20'000, clock), trades, clock);
    book.submit(makeOrder(2, Side::Buy, OrderType::Limit, 40, 20'000, clock), trades, clock);

    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].quantity == 40);
    REQUIRE(book.liveQuantity(OrderId{1}) == std::optional<Quantity>{60}); // resting order partially filled
}

TEST_CASE("incoming order fills across multiple price levels", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Sell, OrderType::Limit, 30, 20'000, clock), trades, clock);
    book.submit(makeOrder(2, Side::Sell, OrderType::Limit, 30, 20'050, clock), trades, clock);
    book.submit(makeOrder(3, Side::Sell, OrderType::Limit, 30, 20'100, clock), trades, clock);

    book.submit(makeOrder(4, Side::Buy, OrderType::Limit, 70, 20'100, clock), trades, clock);

    REQUIRE(trades.size() == 3);
    REQUIRE(trades[0].priceTicks == 20'000);
    REQUIRE(trades[1].priceTicks == 20'050);
    REQUIRE(trades[2].priceTicks == 20'100);
    REQUIRE(trades[2].quantity == 10); // last level only partially consumed
    REQUIRE(book.liveQuantity(OrderId{3}) == std::optional<Quantity>{20});
}

TEST_CASE("residual limit quantity rests", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Sell, OrderType::Limit, 30, 20'000, clock), trades, clock);
    book.submit(makeOrder(2, Side::Buy, OrderType::Limit, 100, 20'000, clock), trades, clock);

    REQUIRE(trades.size() == 1);
    REQUIRE(book.bestBid() == std::optional<PriceTicks>{20'000});
    REQUIRE(book.liveQuantity(OrderId{2}) == std::optional<Quantity>{70});
}

TEST_CASE("residual market quantity does not rest", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Sell, OrderType::Limit, 30, 20'000, clock), trades, clock);
    book.submit(makeOrder(2, Side::Buy, OrderType::Market, 100, std::nullopt, clock), trades, clock);

    REQUIRE(trades.size() == 1);
    REQUIRE(book.bestBid() == std::nullopt); // remaining 70 units of the market order never rest
    REQUIRE(book.liveQuantity(OrderId{2}) == std::nullopt);
}

TEST_CASE("market order against empty book", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Buy, OrderType::Market, 100, std::nullopt, clock), trades, clock);

    REQUIRE(trades.empty());
    REQUIRE(book.bestBid() == std::nullopt);
    REQUIRE(book.liveQuantity(OrderId{1}) == std::nullopt);
}

TEST_CASE("successful cancellation", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Buy, OrderType::Limit, 100, 19'900, clock), trades, clock);
    REQUIRE(book.cancel(OrderId{1}) == CancelResult::Cancelled);
    REQUIRE(book.bestBid() == std::nullopt);
    REQUIRE(book.liveQuantity(OrderId{1}) == std::nullopt);
}

TEST_CASE("cancellation removes correct order while preserving others", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Buy, OrderType::Limit, 50, 19'900, clock), trades, clock);
    book.submit(makeOrder(2, Side::Buy, OrderType::Limit, 50, 19'900, clock), trades, clock);

    REQUIRE(book.cancel(OrderId{1}) == CancelResult::Cancelled);
    REQUIRE(book.liveQuantity(OrderId{2}) == std::optional<Quantity>{50}); // order 2 untouched
    REQUIRE(book.bestBid() == std::optional<PriceTicks>{19'900});          // level still has order 2
}

TEST_CASE("cancellation of unknown ID", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    REQUIRE(book.cancel(OrderId{999}) == CancelResult::UnknownOrder);
}

TEST_CASE("deterministic time/sequence behavior using ManualClock", "[order_book]") {
    ManualClock clock;
    REQUIRE(clock.nextSequence() == 1);
    REQUIRE(clock.nextSequence() == 2);
    REQUIRE(clock.nextSequence() == 3); // strictly increasing, no dependency on wall-clock
}

TEST_CASE("top-of-book after insert/match/cancel", "[order_book]") {
    OrderBook book{Symbol{"AAPL"}};
    ManualClock clock;
    std::vector<Trade> trades;

    book.submit(makeOrder(1, Side::Buy, OrderType::Limit, 50, 19'900, clock), trades, clock);
    REQUIRE(book.bestBid() == std::optional<PriceTicks>{19'900});

    book.submit(makeOrder(2, Side::Buy, OrderType::Limit, 50, 19'950, clock), trades, clock);
    REQUIRE(book.bestBid() == std::optional<PriceTicks>{19'950}); // better bid becomes top

    book.submit(makeOrder(3, Side::Sell, OrderType::Limit, 50, 19'950, clock), trades, clock);
    REQUIRE(book.bestBid() == std::optional<PriceTicks>{19'900}); // top level fully matched away

    REQUIRE(book.cancel(OrderId{1}) == CancelResult::Cancelled);
    REQUIRE(book.bestBid() == std::nullopt);
}

TEST_CASE("order state transitions", "[order_book]") {
    ManualClock clock;
    Order order{OrderId{1}, Symbol{"AAPL"}, Side::Buy, OrderType::Limit, 100, 20'000,
                clock.nextSequence()};
    REQUIRE(order.status() == OrderStatus::New);

    order.fill(40);
    REQUIRE(order.status() == OrderStatus::PartiallyFilled);
    REQUIRE(order.remainingQuantity() == 60);

    order.fill(60);
    REQUIRE(order.status() == OrderStatus::Filled);
    REQUIRE_FALSE(order.isLive());
    REQUIRE_FALSE(order.cancel()); // cannot cancel a fully filled order

    Order restingOrder{OrderId{2}, Symbol{"AAPL"}, Side::Sell, OrderType::Limit, 10, 20'100,
                        clock.nextSequence()};
    REQUIRE(restingOrder.cancel());
    REQUIRE(restingOrder.status() == OrderStatus::Cancelled);
    REQUIRE_THROWS_AS(restingOrder.fill(1), std::logic_error); // cannot fill a cancelled order
}
