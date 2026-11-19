// playground/copy_move_raii.cpp — EDUCATIONAL ONLY.
//
// A tiny owning `Buffer` class that manually manages a dynamic int array
// and implements the full Rule of Five correctly. This is deliberately
// NOT how the main miniq project is written (miniq prefers Rule of Zero
// via std::vector/std::unique_ptr everywhere) — it exists purely so the
// mechanics of copy/move construction, copy/move assignment, and the
// destructor are visible and explainable line by line.
//
// Build/run: ./playground_copy_move_raii
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <utility>
#include <vector>

namespace {

class Buffer {
public:
    explicit Buffer(std::size_t size) : size_(size), data_(new int[size_]{}) {
        std::cout << "  Buffer(size=" << size_ << ") constructed, owns raw array\n";
    }

    // --- destructor: RAII release of the owned resource ---------------------
    ~Buffer() {
        std::cout << "  ~Buffer(size=" << size_ << ") releasing array\n";
        delete[] data_;
    }

    // --- copy constructor: deep copy, never share the pointer ---------------
    Buffer(const Buffer& other) : size_(other.size_), data_(new int[size_]) {
        std::copy(other.data_, other.data_ + size_, data_);
        std::cout << "  Buffer copy-constructed (deep copy)\n";
    }

    // --- copy assignment: copy-and-swap avoids self-assignment bugs and
    // gives the strong exception-safety guarantee "for free" -----------------
    Buffer& operator=(const Buffer& other) {
        std::cout << "  Buffer copy-assigned\n";
        Buffer temp(other);   // may throw; `this` untouched if it does
        swap(temp);           // noexcept, commits the change
        return *this;
    }

    // --- move constructor: steal the pointer, leave source empty-but-valid --
    Buffer(Buffer&& other) noexcept : size_(other.size_), data_(other.data_) {
        other.data_ = nullptr;
        other.size_ = 0;
        std::cout << "  Buffer move-constructed (stole pointer)\n";
    }

    // --- move assignment: release own resource, then steal ------------------
    Buffer& operator=(Buffer&& other) noexcept {
        std::cout << "  Buffer move-assigned\n";
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    void set(std::size_t index, int value) { data_[index] = value; }
    [[nodiscard]] int get(std::size_t index) const { return data_[index]; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

private:
    void swap(Buffer& other) noexcept {
        std::swap(size_, other.size_);
        std::swap(data_, other.data_);
    }

    std::size_t size_;
    int* data_; // owning raw pointer — the whole point of this example

    // Pitfall this class must guard against: double-delete. If move
    // construction/assignment forgot to null out `other.data_`, both the
    // moved-from and moved-to objects would delete[] the same pointer in
    // their destructors -> double delete -> undefined behavior.
};

} // namespace

int main() {
    std::cout << "-- Rule of Five walkthrough --\n";
    Buffer a(4);
    a.set(0, 100);

    std::cout << "-- copy construct b from a --\n";
    Buffer b(a); // copy constructor
    std::cout << "a[0]=" << a.get(0) << " b[0]=" << b.get(0) << " (independent storage)\n";

    std::cout << "-- move construct c from std::move(a) --\n";
    Buffer c(std::move(a)); // move constructor; a is now moved-from
    std::cout << "c[0]=" << c.get(0) << " a.size()=" << a.size()
              << " (a is valid-but-unspecified: safe to destroy/reassign, not to use)\n";

    std::cout << "-- copy assign b = c --\n";
    b = c;

    std::cout << "-- move assign a = std::move(c) --\n";
    a = std::move(c);

    std::cout << "\n-- Contrast: std::vector<int> gets all of this for free (Rule of Zero) --\n";
    std::vector<int> v1{1, 2, 3, 4};
    std::vector<int> v2 = v1;            // deep copy, no manual code needed
    std::vector<int> v3 = std::move(v1); // move, no manual code needed
    std::cout << "v2.size()=" << v2.size() << " v3.size()=" << v3.size()
              << " v1.size()=" << v1.size() << " (moved-from vector is valid-but-unspecified)\n";
    std::cout << "This is why miniq's production code (Order, OrderBook, etc.) relies on\n"
                 "std::vector/std::unique_ptr/std::string and writes NO manual destructors,\n"
                 "copy/move constructors, or assignment operators — Rule of Zero.\n";

    std::cout << "\n-- end of scope: destructors run in reverse declaration order --\n";
    return 0;
}
