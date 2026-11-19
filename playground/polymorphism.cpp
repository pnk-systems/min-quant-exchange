// playground/polymorphism.cpp — EDUCATIONAL ONLY.
//
// Demonstrates OOP pillars, runtime vs compile-time polymorphism, virtual
// dispatch/virtual destructors, object slicing, overload vs override,
// upcasting/downcasting, and — deliberately isolated here because the
// trading domain never needs them — multiple inheritance and the diamond
// problem with virtual inheritance.
//
// Build/run: ./playground_polymorphism
#include <iostream>
#include <memory>
#include <vector>

namespace {

// --- abstraction + interface (pure virtual) ---------------------------------
class Shape {
public:
    virtual ~Shape() = default; // MUST be virtual: see explanation in main()
    [[nodiscard]] virtual double area() const = 0; // pure virtual -> abstract class
    [[nodiscard]] virtual const char* name() const noexcept = 0;
};

// --- inheritance + runtime polymorphism (virtual dispatch) ------------------
class Circle final : public Shape { // `final`: no further derivation allowed
public:
    explicit Circle(double radius) : radius_(radius) {}
    [[nodiscard]] double area() const override { return 3.14159265 * radius_ * radius_; }
    [[nodiscard]] const char* name() const noexcept override { return "Circle"; }

private:
    double radius_;
};

class Rectangle : public Shape {
public:
    Rectangle(double width, double height) : width_(width), height_(height) {}
    [[nodiscard]] double area() const override { return width_ * height_; }
    [[nodiscard]] const char* name() const noexcept override { return "Rectangle"; }

private:
    double width_;
    double height_;
};

// Encapsulation: internal side length is private; only behavior (area(),
// name()) is exposed. Composition/"is-a": Square IS-A Rectangle here for
// brevity, though in stricter designs Square-is-a-Rectangle is debated
// (Liskov: a Square breaks Rectangle's "set width/height independently"
// contract if such setters existed — this Rectangle has none, so it's safe).
class Square final : public Rectangle {
public:
    explicit Square(double side) : Rectangle(side, side) {}
    [[nodiscard]] const char* name() const noexcept override { return "Square"; }
};

// --- compile-time polymorphism: overloading, resolved at compile time -------
[[nodiscard]] double describe(int x) { return x * 1.0; }
[[nodiscard]] double describe(double x) { return x; }
// Overload set is chosen by the compiler from argument types; contrast with
// override, which is chosen by the RUNTIME type behind a base pointer/ref.

// --- composition over inheritance: Engine is a component, not a base -------
class Engine {
public:
    void start() const { std::cout << "  Engine started\n"; }
};

class Car { // "has-a" Engine (composition), not "is-a" Engine (inheritance)
public:
    void start() const { engine_.start(); }

private:
    Engine engine_;
};

// --- object slicing: explained via comment, not invoked, because Shape is
// abstract (cannot be instantiated), which is itself a common way real
// code avoids slicing bugs. If Shape had a concrete field and Circle
// added extra data members, a signature like `void f(Shape s)` would
// still compile for a Circle argument and silently drop everything
// Circle added beyond Shape — that silent truncation is "slicing".

// --- multiple inheritance + diamond problem (playground-only, NOT used in
// the trading domain per the project's design rules) ------------------------
struct Powered {
    virtual ~Powered() = default;
    [[nodiscard]] virtual const char* powerSource() const { return "generic power"; }
};
struct Wheeled {
    virtual ~Wheeled() = default;
    [[nodiscard]] virtual int wheelCount() const { return 4; }
};
// Non-virtual multiple inheritance: fine here because Powered and Wheeled
// do not share a common base, so there is no diamond.
struct Vehicle : Powered, Wheeled {};

// Diamond problem: both Left and Right inherit from Base, and Bottom
// inherits from both. WITHOUT virtual inheritance, Bottom would contain
// TWO separate Base subobjects (ambiguous `Bottom::value()` calls).
struct Base {
    int value{1};
};
struct Left : virtual Base {};   // virtual inheritance -> shared Base subobject
struct Right : virtual Base {};
struct Bottom : Left, Right {};  // exactly ONE Base subobject, thanks to `virtual`

} // namespace

int main() {
    std::cout << "-- Runtime polymorphism via base pointers --\n";
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Circle>(2.0));
    shapes.push_back(std::make_unique<Rectangle>(3.0, 4.0));
    shapes.push_back(std::make_unique<Square>(5.0));
    for (const auto& shape : shapes) {
        // Virtual dispatch: the call is resolved via the vtable at runtime
        // based on the object's ACTUAL type, not the static Shape* type.
        std::cout << "  " << shape->name() << " area=" << shape->area() << '\n';
    }
    std::cout << "Why ~Shape() must be virtual: deleting a Derived object\n"
                 "through a Shape* without a virtual destructor only runs\n"
                 "~Shape(), leaking any Derived-only members -> undefined\n"
                 "behavior for polymorphic deletion.\n";

    std::cout << "\n-- Compile-time polymorphism (overloading) --\n";
    std::cout << "  describe(3)=" << describe(3) << " describe(3.5)=" << describe(3.5) << '\n';

    std::cout << "\n-- Composition over inheritance --\n";
    Car car;
    car.start();

    std::cout << "\n-- Upcasting (implicit, always safe) --\n";
    Circle circle(1.0);
    Shape& upcast = circle; // Derived& -> Base&, implicit, always safe
    std::cout << "  upcast.name()=" << upcast.name() << '\n';

    std::cout << "\n-- Safe downcasting with dynamic_cast, and why it's often avoidable --\n";
    Shape* basePtr = &circle;
    if (auto* asCircle = dynamic_cast<Circle*>(basePtr)) {
        std::cout << "  downcast succeeded, area=" << asCircle->area() << '\n';
    }
    Rectangle rectangle(2.0, 2.0);
    basePtr = &rectangle;
    if (dynamic_cast<Circle*>(basePtr) == nullptr) {
        std::cout << "  downcast to Circle correctly failed for a Rectangle\n";
    }
    std::cout << "  Usually avoidable: prefer adding a virtual method to the\n"
                 "  base interface instead of downcasting + type-switching.\n";

    std::cout << "\n-- Diamond problem resolved with virtual inheritance --\n";
    Bottom bottom;
    bottom.value = 42; // unambiguous: only one Base subobject exists
    std::cout << "  Bottom::value=" << bottom.value << " (single shared Base subobject)\n";

    Vehicle vehicle; // ordinary (non-diamond) multiple inheritance
    std::cout << "  Vehicle: " << vehicle.powerSource() << ", " << vehicle.wheelCount()
              << " wheels\n";

    std::cout << "\nSlicing demo intentionally not invoked: it would require an\n"
                 "instantiable Shape subclass passed by value, which the type\n"
                 "system already discourages by keeping Shape abstract.\n";

    return 0;
}
