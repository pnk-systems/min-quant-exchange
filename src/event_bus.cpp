#include "miniq/event_bus.hpp"

#include <algorithm>

namespace miniq {

void EventBus::subscribe(const std::weak_ptr<IEventListener>& listener) {
    listeners_.push_back(listener);
}

void EventBus::publish(const ExchangeEvent& event) {
    for (auto& weakListener : listeners_) {
        if (auto listener = weakListener.lock()) {
            listener->onEvent(event);
        }
    }
    // Prune expired entries so the vector does not grow unbounded across
    // a long-running process with many short-lived subscribers.
    listeners_.erase(
        std::remove_if(listeners_.begin(), listeners_.end(),
                        [](const std::weak_ptr<IEventListener>& w) { return w.expired(); }),
        listeners_.end());
}

} // namespace miniq
