#include "core/event_bus.h"
#include <algorithm>

namespace ProxyBridge {

SubscriptionId EventBus::subscribe(EventType type, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    SubscriptionId id = m_nextId++;
    m_subscribers[type].push_back({id, std::move(handler)});
    return id;
}

void EventBus::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [type, subs] : m_subscribers) {
        subs.erase(std::remove_if(subs.begin(), subs.end(),
            [id](const Subscription& s) { return s.id == id; }), subs.end());
    }
}

void EventBus::publish(const Event& event) {
    std::vector<EventHandler> handlers;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_subscribers.find(event.type);
        if (it != m_subscribers.end()) {
            for (const auto& sub : it->second) handlers.push_back(sub.handler);
        }
    }
    for (const auto& h : handlers) h(event);
}

void EventBus::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_subscribers.clear();
}

} // namespace ProxyBridge
