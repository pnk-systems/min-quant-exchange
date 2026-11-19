#pragma once
#include <chrono>
#include <cstdint>

namespace miniq {

// IClock — small interface (ISP) exposing exactly what callers need:
// a monotonically increasing sequence number used as the deterministic
// time-priority tie-breaker, plus a wall-clock timestamp for display/audit.
//
// WHY an interface here: tests need reproducible ordering. Injecting
// IClock (DIP) lets ExchangeService/OrderBook stay agnostic of whether
// time comes from the OS or from a scripted ManualClock.
class IClock {
public:
    virtual ~IClock() = default;

    // Monotonic, strictly increasing per call. Used for FIFO tie-breaking
    // within a price level — see OrderBook price-time priority.
    [[nodiscard]] virtual std::uint64_t nextSequence() noexcept = 0;

    [[nodiscard]] virtual std::chrono::system_clock::time_point now() const noexcept = 0;
};

// Production clock: wall-clock time + an atomically-incrementing counter.
class SystemClock final : public IClock {
public:
    [[nodiscard]] std::uint64_t nextSequence() noexcept override { return ++sequence_; }
    [[nodiscard]] std::chrono::system_clock::time_point now() const noexcept override {
        return std::chrono::system_clock::now();
    }

private:
    std::uint64_t sequence_{0};
};

// Test clock: fully deterministic, no dependency on wall-clock time.
// This is what makes tests like "FIFO at same price" reproducible.
class ManualClock final : public IClock {
public:
    [[nodiscard]] std::uint64_t nextSequence() noexcept override { return ++sequence_; }
    [[nodiscard]] std::chrono::system_clock::time_point now() const noexcept override {
        return fixedTime_;
    }

    void advance(std::chrono::seconds delta) noexcept { fixedTime_ += delta; }

private:
    std::uint64_t sequence_{0};
    std::chrono::system_clock::time_point fixedTime_{};
};

} // namespace miniq
