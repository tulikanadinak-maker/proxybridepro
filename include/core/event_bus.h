#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <any>
#include <cstdint>
#include <atomic>

namespace ProxyBridge {

enum class EventType : uint32_t {
    ProxyStarted,
    ProxyStopped,
    IpRotated,
    IpReset,
    ClientConnected,
    ClientDisconnected,
    FrpConnected,
    FrpDisconnected,
    SourceAdded,
    SourceRemoved,
    Error,
    LogEntry,
    StatsUpdated
};

struct Event {
    EventType type;
    std::any data;
    Event(EventType t) : type(t) {}
    Event(EventType t, std::any d) : type(t), data(std::move(d)) {}
};

using EventHandler = std::function<void(const Event&)>;
using SubscriptionId = uint64_t;

class EventBus {
public:
    EventBus() = default;
    ~EventBus() = default;

    SubscriptionId subscribe(EventType type, EventHandler handler);
    void unsubscribe(SubscriptionId id);
    void publish(const Event& event);
    void clear();

private:
    struct Subscription { SubscriptionId id; EventHandler handler; };
    std::unordered_map<EventType, std::vector<Subscription>> m_subscribers;
    mutable std::mutex m_mutex;
    std::atomic<SubscriptionId> m_nextId{1};
};

} // namespace ProxyBridge
