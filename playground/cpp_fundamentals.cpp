// playground/cpp_fundamentals.cpp — EDUCATIONAL ONLY.
//
// Short, runnable demonstrations of core language mechanics that are
// mandatory interview topics but do not need a home in the trading
// domain code: value categories, references vs pointers, const/constexpr,
// storage duration, conversions, enum class, structured bindings,
// range-for, and lambda captures.
//
// Build/run: ./playground_cpp_fundamentals
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace {

// --- declarations vs definitions -------------------------------------------
int add(int a, int b);            // declaration: introduces the name/signature
int add(int a, int b) { return a + b; } // definition: provides the body

// --- pass by value / reference / const reference ----------------------------
void byValue(std::string s) { s += " (local copy, caller unaffected)"; }
void byReference(std::string& s) { s += " (mutated through reference)"; }
void byConstReference(const std::string& s) {
    // s cannot be mutated here — compiler-enforced, zero runtime cost vs
    // pass-by-value for a type as large as std::string.
    std::cout << "  byConstReference sees: " << s << '\n';
}

// --- return by value: NRVO/move typically avoids an extra copy -------------
std::string makeGreeting(const std::string& name) {
    std::string greeting = "Hello, " + name + "!";
    return greeting; // named return value optimization is expected here
}

// --- enum class: scoped, no implicit int conversion -------------------------
enum class TrafficLight : std::uint8_t { Red, Yellow, Green };

[[nodiscard]] constexpr const char* toString(TrafficLight light) noexcept {
    switch (light) {
        case TrafficLight::Red: return "RED";
        case TrafficLight::Yellow: return "YELLOW";
        case TrafficLight::Green: return "GREEN";
    }
    return "?";
}

// --- lvalues/rvalues at interview level -------------------------------------
void describe(int& /*lvalueRef*/) { std::cout << "  bound to an lvalue reference\n"; }
void describe(int&& /*rvalueRef*/) { std::cout << "  bound to an rvalue reference\n"; }

} // namespace

int main() {
    // Compilation units / headers vs source: this file itself is one
    // translation unit; miniq's include/*.hpp + src/*.cpp split shows the
    // header/definition separation in the real project.

    // const vs constexpr
    const int runtimeConst = add(2, 3);        // value fixed at runtime, from a function call
    constexpr int compileTimeConst = 2 + 3;    // must be computable at compile time
    static_assert(compileTimeConst == 5, "compile-time evaluated");
    std::cout << "runtimeConst=" << runtimeConst << " compileTimeConst=" << compileTimeConst
              << '\n';

    // auto + integral types + signed/unsigned pitfall
    auto count = 5;                 // deduced int
    unsigned int limit = 3;
    // Pitfall: `count - limit` promotes count to unsigned if types mismatch
    // in a mixed comparison; prefer same-signedness comparisons.
    if (static_cast<unsigned int>(count) > limit) {
        std::cout << "count exceeds limit (compared as unsigned safely)\n";
    }

    // integer overflow awareness (signed overflow is undefined behavior;
    // this deliberately stays well within range to avoid triggering it)
    std::int32_t nearMax = std::numeric_limits<std::int32_t>::max() - 1;
    std::cout << "nearMax + 1 = " << (nearMax + 1) << " (one below overflow)\n";

    // stack vs dynamic storage
    int stackValue = 42;                          // automatic storage duration
    auto heapValue = std::make_unique<int>(42);    // dynamic storage, RAII-owned
    std::cout << "stackValue=" << stackValue << " *heapValue=" << *heapValue << '\n';

    // pointers, references, nullptr
    int* maybeNull = nullptr;
    int target = 10;
    int& ref = target;      // must bind on creation, cannot be null, cannot rebind
    maybeNull = &target;    // can be reseated, can be null
    std::cout << "ref=" << ref << " *maybeNull=" << *maybeNull << '\n';

    // pass-by-value vs reference vs const-reference
    std::string greeting = "hi";
    byValue(greeting);
    std::cout << "after byValue: " << greeting << '\n'; // unchanged
    byReference(greeting);
    std::cout << "after byReference: " << greeting << '\n'; // changed
    byConstReference(greeting);
    std::cout << makeGreeting("MiniQuantExchange") << '\n';

    // implicit vs explicit conversion
    double implicitlyWidened = 5;      // int -> double, implicit, generally safe
    int explicitlyNarrowed = static_cast<int>(3.99); // explicit, lossy — intent is visible
    std::cout << "implicitlyWidened=" << implicitlyWidened
              << " explicitlyNarrowed=" << explicitlyNarrowed << '\n';

    // enum class
    TrafficLight light = TrafficLight::Green;
    std::cout << "light=" << toString(light) << '\n';

    // structured bindings + std::pair
    std::pair<int, std::string> priceLevel{20100, "best ask"};
    auto [priceTicks, label] = priceLevel;
    std::cout << "structured binding -> priceTicks=" << priceTicks << " label=" << label << '\n';

    // range-for over std::array
    std::array<int, 4> ticks{10, 20, 30, 40};
    int sum = 0;
    for (const int tick : ticks) {
        sum += tick;
    }
    std::cout << "sum of ticks=" << sum << '\n';

    // lambdas and captures: by value, by reference, and generalized capture
    int multiplier = 3;
    auto byValueCapture = [multiplier](int x) { return x * multiplier; };
    auto byRefCapture = [&multiplier](int x) { multiplier += 1; return x * multiplier; };
    auto initCapture = [total = 0](int x) mutable { total += x; return total; };
    std::cout << "byValueCapture(5)=" << byValueCapture(5) << '\n';
    std::cout << "byRefCapture(5)=" << byRefCapture(5) << " (multiplier now " << multiplier
              << ")\n";
    std::cout << "initCapture(1)+initCapture(2)=" << (initCapture(1), initCapture(2)) << '\n';

    // lvalue vs rvalue overload resolution
    int lvalueVar = 7;
    describe(lvalueVar);
    describe(7); // literal is an rvalue

    return 0;
}
