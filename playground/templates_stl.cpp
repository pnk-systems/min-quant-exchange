// playground/templates_stl.cpp — EDUCATIONAL ONLY.
//
// Function templates, class templates, comparator usage, and a tour of
// STL containers/algorithms with complexity/invalidation notes that would
// otherwise clutter the trading-domain code comments. Also demonstrates
// std::optional, std::variant+std::visit, and std::function beyond what
// the main project needs.
//
// Build/run: ./playground_templates_stl
#include <algorithm>
#include <deque>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <queue>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace {

// --- function template -------------------------------------------------------
template <typename T>
[[nodiscard]] const T& maxOf(const T& a, const T& b) {
    return (a < b) ? b : a; // basic template type deduction from call-site args
}

// --- class template (miniq's StrongId<Tag> is the real-project example) -----
template <typename T>
class Box {
public:
    explicit Box(T value) : value_(std::move(value)) {}
    [[nodiscard]] const T& get() const noexcept { return value_; }

private:
    T value_;
};

} // namespace

int main() {
    std::cout << "-- Function/class templates --\n";
    std::cout << "  maxOf(3, 7)=" << maxOf(3, 7) << " maxOf(2.5, 1.5)=" << maxOf(2.5, 1.5) << '\n';
    Box<std::string> box("hello");
    std::cout << "  Box<std::string>.get()=" << box.get() << '\n';
    std::cout << "  Templates give zero-cost abstraction (no runtime indirection),\n"
                 "  at the cost of longer compile times and error messages that\n"
                 "  reference instantiated types rather than the template itself.\n";

    std::cout << "\n-- std::map vs std::unordered_map --\n";
    std::map<int, std::string, std::greater<>> descendingPrices{{100, "a"}, {50, "b"}, {75, "c"}};
    std::cout << "  map (greater<>) iteration order: ";
    for (const auto& [price, label] : descendingPrices) std::cout << price << ' ';
    std::cout << "\n  map: O(log n) lookup/insert/erase, ordered iteration, node-based\n"
                 "       (pointer-chasing, poor cache locality).\n"
                 "  unordered_map: average O(1) lookup/insert/erase, no ordering,\n"
                 "       bucket+node based; used in miniq for OrderId -> Locator\n"
                 "       lookup where insertion order is irrelevant and speed matters.\n";

    std::cout << "\n-- vector vs list vs deque --\n";
    std::vector<int> vec{1, 2, 3};
    vec.reserve(10); // preallocation avoids repeated reallocation/copy
    vec.push_back(4);
    std::cout << "  vector: contiguous storage, O(1) amortized push_back, O(n) insert\n"
                 "          in the middle, best cache locality; push_back may invalidate\n"
                 "          ALL iterators/references if it reallocates.\n";
    std::list<int> lst{1, 2, 3};
    auto lit = lst.begin();
    lst.push_back(4); // does NOT invalidate existing iterators (unlike vector)
    std::cout << "  list: node-based, O(1) insert/erase given an iterator, stable\n"
                 "        iterators across insert/erase elsewhere; worse cache locality.\n"
                 "        miniq's OrderBook price-level uses std::list<Order> exactly\n"
                 "        for this O(1)-erase-via-iterator + FIFO property.\n";
    std::deque<int> dq{1, 2, 3};
    dq.push_front(0); // O(1) at both ends, unlike vector
    std::cout << "  deque: O(1) push at both ends, random access like vector, but NOT\n"
                 "         contiguous as a single block -> weaker cache locality than vector.\n";
    (void)lit;

    std::cout << "\n-- iterators, find, sort, lower_bound, remove_if, erase, emplace --\n";
    std::vector<int> nums{5, 3, 1, 4, 1, 5, 9, 2, 6};
    std::sort(nums.begin(), nums.end());
    auto found = std::find(nums.begin(), nums.end(), 4);
    std::cout << "  sorted, find(4) found=" << (found != nums.end()) << '\n';
    auto lb = std::lower_bound(nums.begin(), nums.end(), 5);
    std::cout << "  lower_bound(5) -> first element >=5 is " << *lb << '\n';
    // erase-remove idiom: remove_if partitions, erase actually shrinks.
    nums.erase(std::remove_if(nums.begin(), nums.end(), [](int n) { return n % 2 == 0; }),
               nums.end());
    std::cout << "  after erase-remove of evens, size=" << nums.size() << '\n';
    std::vector<std::pair<int, std::string>> pairs;
    pairs.emplace_back(1, "one"); // constructs in place, no temporary pair
    std::cout << "  emplace_back avoided a temporary std::pair construction+move\n";

    std::cout << "\n-- std::queue / std::stack / std::priority_queue --\n";
    std::queue<int> fifo;
    fifo.push(1);
    fifo.push(2);
    std::cout << "  queue.front()=" << fifo.front() << " (FIFO, used conceptually like\n"
                 "  miniq's price-level order list)\n";
    std::stack<int> lifo;
    lifo.push(1);
    lifo.push(2);
    std::cout << "  stack.top()=" << lifo.top() << " (LIFO)\n";
    std::priority_queue<int> maxHeap;
    maxHeap.push(3);
    maxHeap.push(9);
    maxHeap.push(1);
    std::cout << "  priority_queue.top()=" << maxHeap.top() << " (always the max)\n";

    std::cout << "\n-- std::optional --\n";
    std::optional<int> maybePrice;
    std::cout << "  empty optional has_value()=" << maybePrice.has_value() << '\n';
    maybePrice = 20100;
    std::cout << "  after assignment, value_or(-1)=" << maybePrice.value_or(-1) << '\n';

    std::cout << "\n-- std::variant + std::visit --\n";
    std::variant<int, std::string> v = 42;
    std::visit([](const auto& value) { std::cout << "  variant holds: " << value << '\n'; }, v);
    v = std::string("now a string");
    std::visit([](const auto& value) { std::cout << "  variant holds: " << value << '\n'; }, v);
    std::cout << "  miniq's ExchangeEvent uses variant+visit for a closed, exhaustively\n"
                 "  checkable event set (see ConsolePrinter in src/main.cpp).\n";

    std::cout << "\n-- std::function --\n";
    std::function<int(int, int)> op = [](int a, int b) { return a + b; };
    std::cout << "  std::function wrapping a lambda: op(2,3)=" << op(2, 3) << '\n';
    std::cout << "  Cost: type-erasure means a possible heap allocation + virtual-call-\n"
                 "  like indirection, vs a template callable parameter which the\n"
                 "  compiler can inline. Prefer templates on hot paths, std::function\n"
                 "  for storage/heterogeneity (e.g. a callback registry).\n";

    return 0;
}
