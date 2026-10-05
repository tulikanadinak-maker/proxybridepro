#pragma once

/**
 * @file subnet_pool.h
 * @brief Pool of IPv6 addresses from multiple subnets
 *
 * Aggregates addresses from all active sources into a unified
 * pool for the proxy server to draw from.
 */

#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <optional>
#include <cstdint>

namespace ProxyBridge {

class SourceManager;
class IPv6Manager;

struct PoolEntry {
    std::string address;
    uint32_t sourceId;
    uint64_t useCount = 0;
    bool bound = false;
    bool inUse = false;
};

class SubnetPool {
public:
    explicit SubnetPool(SourceManager& sourceManager);
    ~SubnetPool();

    /**
     * @brief Build the pool from all active sources
     * @return true if pool was built successfully
     */
    bool build();

    /**
     * @brief Rebuild pool (rotate all addresses)
     */
    void rebuild();

    /**
     * @brief Get next address from pool (round-robin)
     * @return IPv6 address string
     */
    std::string getNext();

    /**
     * @brief Get pool size
     */
    size_t size() const;

    /**
     * @brief Get all entries
     */
    std::vector<PoolEntry> entries() const;

    /**
     * @brief Clear and unbind all
     */
    void clear();

    /**
     * @brief Bind all unbound pool addresses via IPv6Manager
     */
    void bindAll();

    /**
     * @brief Acquire the next bound, unused entry (by value - never dangling)
     * @return Copy of the entry, or nullopt if none available
     */
    std::optional<PoolEntry> acquire();

    /**
     * @brief Release a previously acquired entry
     */
    void release(const PoolEntry& entry);

    /**
     * @brief Check if pool is active
     */
    bool isActive() const;

private:
    SourceManager& m_sourceManager;
    std::vector<PoolEntry> m_pool;
    mutable std::mutex m_mutex;
    std::atomic<uint64_t> m_index{0};
    std::atomic<bool> m_active{false};
};

} // namespace ProxyBridge
