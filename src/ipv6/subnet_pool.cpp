#include "ipv6/subnet_pool.h"
#include "ipv6/source_manager.h"
#include "ipv6/ipv6_manager.h"
#include "core/application.h"

namespace ProxyBridge {

SubnetPool::SubnetPool(SourceManager& sourceManager)
    : m_sourceManager(sourceManager) {}

SubnetPool::~SubnetPool() { clear(); }

bool SubnetPool::build() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pool.clear();

    auto sources = m_sourceManager.getAllSources();
    for (const auto& src : sources) {
        if (!src.enabled) continue;

        for (uint32_t i = 0; i < src.slots; ++i) {
            IPv6Address addr = IPv6Address::random(src.subnet.prefix, src.subnet.prefixLength);
            PoolEntry entry;
            entry.address = addr.toString();
            entry.sourceId = src.id;
            entry.bound = false;  // not yet bound to an interface
            m_pool.push_back(std::move(entry));
        }
    }

    m_active = !m_pool.empty();
    m_index = 0;
    return m_active;
}

void SubnetPool::rebuild() {
    // Single critical section - clear() also takes m_mutex, calling it from
    // here would deadlock with the non-recursive mutex.
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pool.clear();
    m_active = false;
    m_index = 0;

    auto sources = m_sourceManager.getAllSources();
    for (const auto& src : sources) {
        if (!src.enabled) continue;

        for (uint32_t i = 0; i < src.slots; ++i) {
            IPv6Address addr = IPv6Address::random(src.subnet.prefix, src.subnet.prefixLength);
            PoolEntry entry;
            entry.address = addr.toString();
            entry.sourceId = src.id;
            entry.bound = false;
            m_pool.push_back(std::move(entry));
        }
    }

    m_active = !m_pool.empty();
}

void SubnetPool::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pool.clear();
    m_active = false;
}

void SubnetPool::bindAll() {
    // Bind addresses via IPv6Manager without holding the pool lock while the
    // (slow, process-spawning) bind calls run.
    std::vector<PoolEntry> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        snapshot = m_pool;
    }

    auto& manager = Application::instance().ipv6Manager();
    bool anyBound = false;
    for (auto& entry : snapshot) {
        IPv6Address addr;
        if (!addr.fromString(entry.address)) {
            entry.bound = false;
            continue;
        }
        // Interface name comes from the source configuration; use the
        // manager's configured interface as the bind target.
        entry.bound = manager.bindAddress(addr, manager.subnet().ifaceName);
        if (entry.bound) anyBound = true;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!snapshot.empty()) m_pool = snapshot;
    m_active = anyBound || m_active;
}

std::string SubnetPool::getNext() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pool.empty()) return "";

    // Round-robin over entries, preferring bound ones.
    for (size_t attempts = 0; attempts < m_pool.size(); ++attempts) {
        uint64_t idx = m_index.fetch_add(1) % m_pool.size();
        auto& entry = m_pool[static_cast<size_t>(idx)];
        if (!entry.bound) continue;
        entry.useCount++;
        return entry.address;
    }
    return "";  // nothing bound
}

std::optional<PoolEntry> SubnetPool::acquire() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pool.empty()) return std::nullopt;

    for (size_t attempts = 0; attempts < m_pool.size(); ++attempts) {
        uint64_t idx = m_index.fetch_add(1) % m_pool.size();
        auto& entry = m_pool[static_cast<size_t>(idx)];
        if (!entry.bound || entry.inUse) continue;
        entry.inUse = true;
        entry.useCount++;
        PoolEntry out = entry;  // by value - caller never holds a reference
        return out;
    }
    return std::nullopt;
}

void SubnetPool::release(const PoolEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& e : m_pool) {
        if (e.address == entry.address && e.sourceId == entry.sourceId) {
            e.inUse = false;
            return;
        }
    }
}

size_t SubnetPool::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pool.size();
}

std::vector<PoolEntry> SubnetPool::entries() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pool;
}

bool SubnetPool::isActive() const { return m_active; }

} // namespace ProxyBridge
