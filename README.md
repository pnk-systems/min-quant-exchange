# MiniQuantExchange

A small, deterministic single-threaded limit order book and matching engine
written in modern C++20, built as a low-level-design (LLD) and C++
fundamentals learning project for quant/trading interview preparation.

## What it does

MiniQuantExchange accepts LIMIT and MARKET order requests for one or more
symbols, matches them against a resting order book using **price-time
priority**, applies pre-trade **risk checks**, tracks a running **position**,
and publishes **events** (accepted, rejected, cancelled, trade, book update)
to any subscribed listener. A tiny `ITradingStrategy` example shows how a
strategy can react to top-of-book state and submit its own orders.

## Why it exists

The project's purpose is to be *thoroughly interview-discussable*: every
class maps to a specific C++, OOP, SOLID, or LLD interview topic, and
`docs/INTERVIEW_LEARNING_GUIDE.md` indexes every one of them back to exact
code. See that file for the full revision route; this README stays focused
on running the code.

## Architecture at a glance

```
OrderRequest --> ExchangeService --> RiskEngine (accept/reject)
                       |
                       v
                MatchingEngine --> OrderBook (per symbol, price-time priority)
                       |
                       v
                  Trade(s) --> PositionService (position tracking)
                       |
                       v
                  EventBus --> IEventListener (Observer/Pub-Sub)
```

`ExchangeService` is the orchestration boundary: it validates a request,
checks risk against the current position, hands accepted orders to
`MatchingEngine`, updates `PositionService` from resulting trades, and
publishes `ExchangeEvent`s through `EventBus`. It depends on all of these
collaborators only through constructor injection (DIP), which is what makes
the whole thing testable with a `ManualClock` and no real time dependency.

## Quickest start: `run.sh`

A single script wraps every cmake/build/run command below. No flags to
remember:

```bash
./run.sh            # configure + build + run tests + run demo + run playground
./run.sh build      # configure + build only
./run.sh test       # build (if needed) + run the test suite
./run.sh demo       # build (if needed) + run the miniq_demo CLI
./run.sh playground # build (if needed) + run all 5 playground executables
./run.sh asan       # configure + build with ASan/UBSan + run tests under it
./run.sh clean      # remove build directories
```

Requires CMake 3.20+, a C++20 compiler (tested with GCC 13), and network
access on first configure (Catch2 is fetched via `FetchContent`).

## Build (manual, without run.sh)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

To build with AddressSanitizer + UndefinedBehaviorSanitizer:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DMINIQ_ENABLE_SANITIZERS=ON
cmake --build build-asan -j
```

## Test

```bash
cd build
./miniq_tests
# or, once installed:
ctest --output-on-failure
```

Current status: **32 test cases, 96 assertions, all passing**, clean under
`-Wall -Wextra -Wpedantic` and under ASan/UBSan.

## Demo

```bash
./build/miniq_demo
```

## Sample output

```
== Resting orders ==
[ACCEPTED]  order=1 symbol=AAPL
[BOOK]      symbol=AAPL bid=19900 ask=-
[ACCEPTED]  order=2 symbol=AAPL
[BOOK]      symbol=AAPL bid=19900 ask=20100

== Crossing order ==
[ACCEPTED]  order=3 symbol=AAPL
[TRADE]     aggressor=3 resting=2 price=20100 qty=150
[BOOK]      symbol=AAPL bid=20100 ask=-

== Cancel remaining resting order ==
[CANCELLED] order=1 symbol=AAPL
[BOOK]      symbol=AAPL bid=20100 ask=-

== Strategy demo (PassiveJoinStrategy) ==
[ACCEPTED]  order=4 symbol=AAPL
[BOOK]      symbol=AAPL bid=20100 ask=20200
[ACCEPTED]  order=100 symbol=AAPL
[BOOK]      symbol=AAPL bid=20199 ask=20200

Final position(AAPL) = 150
```

There are also five standalone `playground/` executables
(`playground_cpp_fundamentals`, `playground_copy_move_raii`,
`playground_polymorphism`, `playground_templates_stl`,
`playground_concurrency`) that demonstrate C++ concepts kept out of the
trading domain code — see `docs/INTERVIEW_LEARNING_GUIDE.md` §Playground.

## Directory structure

```
MiniQuantExchange/
├── CMakeLists.txt
├── README.md
├── include/miniq/       -- public headers (one concept-bearing class per file)
├── src/                 -- implementations + main.cpp demo
├── tests/                -- Catch2 tests (28 required behaviors covered)
├── playground/           -- standalone files for concepts kept out of the domain
└── docs/
    ├── INTERVIEW_LEARNING_GUIDE.md   -- the main revision artifact
    └── DESIGN_DECISIONS.md           -- ADR-style rationale
```

## Supported order flow

1. Client builds an `OrderRequest` (symbol, side, type, quantity, optional
   limit price) and calls `ExchangeService::submit`.
2. Request is validated (quantity > 0; limit orders require a positive
   price), checked for a duplicate ID, then run through `RiskEngine`.
3. On acceptance, `MatchingEngine` routes the order to its symbol's
   `OrderBook`, which matches against the opposite side at the best
   available price(s), oldest order first at each price level.
4. Any unfilled LIMIT remainder rests on the book; an unfilled MARKET
   remainder is dropped (documented policy, not silently lost).
5. Every resulting trade updates the aggressor's position and is published
   as a `TradeExecuted` event; `OrderAccepted`/`OrderRejected`/
   `OrderCancelled`/`BookUpdated` are published at the relevant points.
6. `ExchangeService::cancel` removes a still-live resting order and
   publishes `OrderCancelled` + a fresh `BookUpdated`.

## Key design decisions

See `docs/DESIGN_DECISIONS.md` for the full ADR-style writeups. Highlights:

- Prices are integer ticks (`PriceTicks = std::int64_t`), never floating
  point, for exact ordering/equality.
- `bids_`/`asks_` are `std::map<PriceTicks, PriceLevel, Compare>` for
  ordered best-price access; each price level is a `std::list<Order>` for
  O(1) FIFO erase-by-iterator; an `unordered_map<OrderId, Locator>` gives
  O(1) average cancel lookup.
- The matching core is deliberately single-threaded for determinism; see
  the guide's "if an interviewer asks about production/low-latency"
  section for what would change.
- Production code follows the Rule of Zero throughout; a manually
  Rule-of-Five `Buffer` class lives only in `playground/copy_move_raii.cpp`
  as a contrast.
- Domain rejections (invalid quantity, risk limits, unknown symbol, etc.)
  are explicit result values (`RejectReason`, `CancelResult`), not
  exceptions — see the guide's error-handling section for why.

## Limitations / non-goals

- Single-account position model: `PositionService` tracks one implicit
  trading account per symbol, not per-client positions. See
  `docs/DESIGN_DECISIONS.md`.
- No persistence, no network layer, no multi-threading in the matching
  core — this is a correctness-and-design teaching project, not a
  production exchange. The concurrency/low-latency sections in the guide
  discuss what a production version would add.
- `MaxNotionalRule` does not evaluate MARKET orders (notional is unknown
  pre-match); documented, not silently skipped.
