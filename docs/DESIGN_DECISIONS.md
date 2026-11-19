# Design Decisions

ADR-style writeups for the main architectural choices in MiniQuantExchange.
Each entry: Context, Choice, Alternatives, Trade-offs, Interview takeaway.

---

## 1. Integer ticks instead of floating-point price

**Context.** Prices need exact ordering, exact equality, and exact
arithmetic for notional checks.

**Choice.** `PriceTicks = std::int64_t` (`include/miniq/types.hpp`). A
"tick" represents the smallest price increment (e.g. cents); the CLI demo
uses 19'900/20'100 etc. to mean $199.00/$201.00.

**Alternatives.** `double`/`float`; a fixed-point decimal class;
`long double`.

**Trade-offs.** Floating point cannot represent most decimal fractions
exactly (`0.1 + 0.2 != 0.3`), which breaks price-level equality
(`std::map` keying) and can silently misprice trades. Integer ticks trade
away fractional flexibility (you must pick a tick size up front) for exact,
deterministic comparisons and hashing.

**Interview takeaway.** "Why not just use double for price?" is a classic
question — the answer is exact equality/ordering for map keys and
arithmetic, not performance.

---

## 2. Order-book container choices

**Context.** The book must support: best-price lookup, insertion at a
price, matching at the best price, O(1)-ish cancellation by ID, and FIFO
within a price level.

**Choice** (`include/miniq/order_book.hpp`):
- `bids_: std::map<PriceTicks, PriceLevel, std::greater<>>` — `begin()` is
  always the best (highest) bid.
- `asks_: std::map<PriceTicks, PriceLevel, std::less<>>` — `begin()` is
  always the best (lowest) ask.
- `PriceLevel::orders: std::list<Order>` — FIFO via `push_back`/`front()`,
  and O(1) erase given an iterator (used by cancel and by a fully-filled
  resting order).
- `locators_: std::unordered_map<OrderId, Locator>` where `Locator` stores
  the side, price, and exact `std::list<Order>::iterator` — average O(1)
  cancel lookup without scanning price levels.

**Alternatives.** `std::vector<Order>` per level (contiguous, cache
friendly, but O(n) erase from the middle and iterator invalidation on
erase); `std::deque<Order>` (similar erase cost to vector in the middle,
O(1) at the ends, weaker cache locality than vector); a flat array indexed
by price for very tight tick ranges (fast, but wastes memory for sparse
books and needs a known bounded price range).

**Trade-offs.** `std::list` gives the cancellation properties this project
wants "for free" at the cost of a node allocation per order and poor cache
locality versus a contiguous container — acceptable for an educational,
correctness-first project; not what a real low-latency book would use (see
§9). `std::map` gives ordered best-price access in O(log L) (L = distinct
price levels) at the cost of pointer-chasing versus a flat/array-indexed
structure.

**Complexity summary:**
| Operation | Complexity |
|---|---|
| best price lookup | O(1) (`map::begin()`) |
| insertion (new price level) | O(log L) |
| insertion (existing price level) | O(log L) + O(1) list push_back |
| match at best price | O(1) amortized per resting order matched |
| cancellation by ID | O(1) average (`unordered_map` + list iterator) |
| symbol lookup | O(1) average (`MatchingEngine`'s `unordered_map<string, OrderBook>`) |
| risk-rule loop | O(R) for R registered rules, each O(1) |

**Interview takeaway.** Be ready to justify container choice against the
*specific operations* the interviewer cares about, and to say plainly this
is an educational choice, not necessarily what a production HFT exchange
uses.

---

## 3. Single-threaded matching core

**Context.** Correctness and determinism (reproducible test ordering,
reproducible FIFO) versus throughput.

**Choice.** `OrderBook`/`MatchingEngine`/`ExchangeService` are all called
synchronously from a single logical thread; no locks anywhere in the
matching path.

**Alternatives.** Lock-protected shared book across matching threads;
sharded/partitioned books processed by dedicated matching threads with
message passing; fully lock-free structures.

**Trade-offs.** Single-threaded gives simple, provably-correct FIFO/price-
time priority and trivially reproducible tests. It caps throughput to one
core's worth of matching for a given symbol. A production system typically
gets concurrency by *partitioning by symbol* (one matching thread per
symbol/shard) rather than by locking a single book, which avoids most
lock contention entirely rather than trying to make locking fast.

**Interview takeaway.** "How would you make this concurrent?" — answer
with partitioning by symbol first, not "add a mutex to OrderBook."

---

## 4. Rule of Zero in production code

**Context.** Resource management correctness (no leaks, no double-frees,
no dangling references) across copy/move.

**Choice.** Every production class (`Order`, `Trade`, `OrderBook`, etc.)
either has no custom destructor/copy/move (Rule of Zero, relying on
`std::string`/`std::vector`/`std::unique_ptr`/`std::map` members to manage
their own resources) or is explicitly move-only where copying would be
unsafe (`OrderBook` deletes copy, defaults move — see
`include/miniq/order_book.hpp`).

**Alternatives.** Manually writing destructor/copy/move for classes that
own a resource directly (Rule of Three/Five).

**Trade-offs.** Rule of Zero means less code and fewer opportunities for
double-delete/leak bugs, at the cost of needing to actually understand
*why* it works (i.e. that every member's own special members already do
the right thing). `playground/copy_move_raii.cpp` contains the one
deliberate Rule-of-Five example in the repo, explicitly labeled
educational-only, to contrast with this.

**Interview takeaway.** Rule of Zero is the *default* to reach for;
Rule of Five is what you write when a class directly owns a raw resource
(and even then, prefer wrapping the raw resource in a Rule-of-Five class
exactly once, then compose it — don't repeat the pattern everywhere).

---

## 5. Where unique_ptr / shared_ptr / weak_ptr / references / values are used

- `std::unique_ptr<IRiskRule>` in `RiskEngine::rules_` — exclusive
  ownership; no other component needs to share or outlive a rule.
- `std::weak_ptr<IEventListener>` in `EventBus::listeners_` — the bus must
  never keep a listener alive past its owner's lifetime (a classic
  broadcaster/subscriber lifetime problem); `shared_ptr` is held by
  whoever owns the listener (e.g. `main.cpp`'s `ConsolePrinter`).
- References (`IClock&`, `MatchingEngine&`, etc.) in `ExchangeService`'s
  constructor-injected dependencies — non-owning, non-null, non-reseatable
  collaborators with a lifetime the caller guarantees outlives the service.
- Values everywhere else (`Order`, `Trade`, `Symbol`, `OrderRequest`) —
  small, independently-owned data that is naturally passed/stored by
  value with move semantics doing the heavy lifting.

**Interview takeaway.** Choosing a smart pointer (or none) is about
*ownership semantics*, not defaulting to `shared_ptr` everywhere — each
choice above maps to a specific ownership question.

---

## 6. Risk rules use runtime polymorphism

**Context.** New risk policies should be addable without modifying
`RiskEngine`.

**Choice.** `IRiskRule` is an abstract interface; `RiskEngine` holds
`std::vector<std::unique_ptr<IRiskRule>>` and evaluates them in insertion
order (`include/miniq/risk.hpp`, `src/risk.cpp`).

**Alternatives.** `std::function<std::optional<RejectReason>(const
RiskContext&)>` per rule (avoids a vtable, but loses named types for
richer future rules); `std::variant<MaxQuantityRule, MaxNotionalRule,
...>` + `std::visit` (closed set — adding a new rule means touching every
visitor, exactly the OCP violation this design avoids).

**Trade-offs.** Virtual dispatch costs one indirect call per rule per
order — negligible at this scale, and it buys true open/closed extension:
a new `IRiskRule` implementation needs zero changes to `RiskEngine`.

**Interview takeaway.** This is the textbook Strategy pattern answer to
"how would you add a new risk check without touching existing code?"

---

## 7. Events use Observer + std::variant

**Context.** Multiple, decoupled listeners (logging, position updates,
UI, strategies) need to react to exchange activity without `ExchangeService`
knowing who they are.

**Choice.** `EventBus` (Observer/Pub-Sub) broadcasts `ExchangeEvent =
std::variant<OrderAccepted, OrderRejected, OrderCancelled, TradeExecuted,
BookUpdated>` (`include/miniq/events.hpp`, `include/miniq/event_bus.hpp`).

**Alternatives.** A polymorphic `Event` base class with derived event
types (`OrderAcceptedEvent : Event`, etc.); separate typed callback
signatures per event kind (`onTrade`, `onReject`, ...).

**Trade-offs.** `std::variant` + `std::visit` models a *closed* set of
event kinds: an exhaustive `std::visit` (as in `main.cpp`'s
`ConsolePrinter`) is a **compile error** if a new event variant is added
and a case is missing — a base-class hierarchy with a default branch would
instead silently swallow the new event type at runtime. The cost is that
`ExchangeEvent` cannot be extended by code outside this project (a truly
open set would need the base-class approach instead).

**Interview takeaway.** variant+visit vs. inheritance-based events is a
closed-set-vs-open-set design trade-off, not just a style preference.

---

## 8. Errors are explicit domain results, not exceptions

**Context.** Order rejection, cancellation-of-unknown-ID, and similar
outcomes happen routinely in normal trading flow, not exceptionally.

**Choice.** `SubmitResult{accepted, rejectReason, trades}` and
`CancelResult` enum are returned by value; exceptions
(`std::invalid_argument`, `std::logic_error`) are reserved for *programmer
errors*/broken invariants (e.g. `Order::fill()` on a non-live order, which
should never happen if `OrderBook` is internally consistent).

**Alternatives.** Throwing a custom `RejectedOrderException` for every
rejection; `std::expected<T, E>`-style result type (not used here since
the project targets plain C++20 without relying on a C++23-only type, per
the prompt's constraint).

**Trade-offs.** Exceptions are relatively expensive and, more importantly,
communicate "something went wrong that the caller likely didn't plan for."
A rejected order is a normal, expected branch of business logic — modeling
it as a return value keeps the happy/rejected paths equally visible at the
call site and equally cheap. Exceptions remain appropriate for invariant
violations, where by definition the caller's assumptions were already
broken.

**Interview takeaway.** "When do you use exceptions vs. return codes?" —
exceptions for exceptional/programmer-error conditions, explicit result
types for expected, routine business outcomes.

---

## 9. Why some C++ concepts are isolated in playground examples

**Context.** The mandatory C++ concept list (multiple inheritance/diamond
problem, manual Rule-of-Five resource management, raw concurrency
primitives, a broad STL tour) does not all belong naturally in a small,
focused trading domain.

**Choice.** `playground/cpp_fundamentals.cpp`, `copy_move_raii.cpp`,
`polymorphism.cpp`, `templates_stl.cpp`, and `concurrency.cpp` hold these
demonstrations as standalone, clearly-labeled educational executables,
separate from `miniq_core`.

**Alternatives.** Force every concept into the domain model (e.g. add a
diamond-inheritance hierarchy to `Order` for no domain reason).

**Trade-offs.** Keeping the domain code focused makes it a better example
of *when* to reach for a given technique, not just *how*; the cost is an
extra five small files to maintain, and a reader has to know to look in
two places for full concept coverage — mitigated by the index in
`docs/INTERVIEW_LEARNING_GUIDE.md`.

**Interview takeaway.** Forcing unrelated language features into a design
because "the requirements say to demo them" is itself an anti-pattern —
recognizing when a concept doesn't belong is as valuable as knowing the
concept.

---

## 10. What would change under low-latency production requirements

Marked explicitly as **design discussion, not implemented claims**:

- Replace `std::list<Order>` price levels with a flat/contiguous,
  preallocated structure (e.g. a ring buffer or intrusive list over a
  pool) to remove per-order node allocation and improve cache locality.
- Single-writer matching thread per symbol (partition by symbol) instead
  of a single global matching thread, to scale throughput without
  introducing lock contention on a shared book.
- Bounded, preallocated queues between an I/O/gateway thread and the
  matching thread, to avoid allocation and unbounded memory growth under
  load.
- Minimize allocations on the hot path generally (reserve capacity,
  object pools for `Order`/`Trade`, avoid `std::string`/`Symbol`
  construction per message — intern symbols once).
- Reconsider virtual dispatch on the hottest paths (`IRiskRule::evaluate`)
  if profiling shows it matters; likely still fine given rule counts are
  small, but this is something to measure, not assume.
- Careful telemetry and deterministic replay (log every input
  deterministically so a production incident can be replayed exactly)
  built in from the start rather than bolted on.
- Benchmark/profile before optimizing any of the above — "why measuring
  beats guessing" applies here as much as anywhere.
- Separate network I/O from matching entirely (matching thread only ever
  touches in-memory queues, never sockets).
- Avoid locks on the hot path where the partitioning-by-symbol
  architecture makes them unnecessary.

None of this is implemented in MiniQuantExchange — it exists so the
project can host an informed conversation about the gap between an
educational, correctness-first design and a production, latency-sensitive
one.
