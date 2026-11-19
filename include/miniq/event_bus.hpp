#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "miniq/events.hpp"

namespace miniq {

// IEventListener — Observer pattern, consumer side. Interface is
// deliberately tiny (ISP): the only thing a listener does is receive
// events, nothing else.
class IEventListener {
public:
    virtual ~IEventListener() = default;
    virtual void onEvent(const ExchangeEvent& event) = 0;
};

// EventBus — Publish/Subscribe. Listeners are held by std::weak_ptr so the
// bus never keeps a listener alive past its owner's lifetime and never
// crashes dereferencing a dangling listener (see test #23: "expired weak
// listener does not crash event bus"). This is the project's main
// motivating, realistic use of weak_ptr: broadcast systems are the classic
// case where the publisher must NOT extend subscriber lifetime.
class EventBus {
public:
    void subscribe(const std::weak_ptr<IEventListener>& listener);

    // Publishes to every still-alive subscriber; silently prunes expired
    // ones. O(n) in current subscriber count, which is fine at this scale
    // — see docs/DESIGN_DECISIONS.md for what would change under load.
    void publish(const ExchangeEvent& event);

private:
    std::vector<std::weak_ptr<IEventListener>> listeners_;
};

} // namespace miniq
