#pragma once

/**
 * @file ipv6_manager.h
 * @brief Core IPv6 address management and rotation
 *
 * Manages IPv6 subnets, generates random addresses from prefix,
 * binds them to interfaces, and handles rotation logic.
 */

#include <string>
#include <vector>
#include <array>
#include <mutex>
#include <atomic>
#include <random>
#include <chrono>
#include <cstdint>
#include <optional>
#include <utility>

// Windows COM headers define 'interface' as a macro - undo it
#ifdef interface
#undef interface
#endif

namespace ProxyBridge {

/**
 * @brief Represents an IPv6 address (128 bits)
 */
struct IPv6Address {
    std::array<uint8_t, 16> bytes{};

    std::string toString() const;
    static std::optional<IPv6Address> fromString(const std::string& str);
    static IPv6Address random(const IPv6Address& prefix, int prefixLen);
    bool operator==(const IPv6Address& other) const;
};

/**
 * @brief IPv6 subnet configuration
 */
struct IPv6Subnet {
    IPv6Address prefix;
    int prefixLength = 64;   // /48, /56, /64
    std::string ifaceName;    // Network interface name
    std::string gateway;      // Default gateway
    uint32_t validLifetimeSec = 0;  // remaining RA lifetime (larger = newer prefix)
    bool autoOrigin = false;  // true = prefix came from SLAAC/RA or DHCP (ISP-delegated,
                              // routable). false = Manual/statis (NOT routable as pool base).
    uint64_t ifaceLuid = 0;   // Windows interface LUID (used to scope stale-address purge)
};

/**
 * @brief Active IPv6 slot - a bound address ready for use
 */
struct IPv6Slot {
    uint32_t id;
    IPv6Address address;
    std::string ifaceName;
    bool active = false;
    std::chrono::steady_clock::time_point createdAt;
    uint64_t requestCount = 0;
    uint64_t bytesTransferred = 0;
    // #19: quarantine bookkeeping. A slot whose bind the OS overrode is parked
    // for a while instead of serving every request with the wrong source IP.
    int bindFailures = 0;
    std::chrono::steady_clock::time_point quarantinedUntil{};
};

/**
 * @class IPv6Manager
 * @brief Manages IPv6 address pool and rotation
 *
 * Generates random IPv6 addresses from configured subnets,
 * binds them to network interfaces, and rotates on demand.
 */
class IPv6Manager {
public:
    IPv6Manager();
    ~IPv6Manager();

    /**
     * @brief Initialize with a subnet
     * @param subnet IPv6 subnet to use
     * @param slotCount Number of IPs to generate and bind
     * @return true if initialization succeeded
     */
    bool initialize(const IPv6Subnet& subnet, uint32_t slotCount = 50);

    /**
     * @brief Start the IPv6 pool (bind addresses to interface)
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop and unbind all addresses
     */
    void stop();

    /**
     * @brief Rotate all IPs - generate new random addresses
     */
    void rotateAll();

    /**
     * @brief Reset to original addresses
     */
    void resetAll();

    /**
     * @brief Get next available IPv6 address for outbound connection
     * @return IPv6 address string, or empty if none available
     */
    std::string getNextAddress();

    /**
     * @brief Get next available IPv6 address together with its slot id
     * @return (slotId, address string), or nullopt if no active slot
     */
    std::optional<std::pair<uint32_t, std::string>> getNextAddressSlot();

    /**
     * @brief Get all active slots
     * @return Vector of active slots
     */
    std::vector<IPv6Slot> getActiveSlots() const;

    /**
     * @brief Get current slot count
     */
    uint32_t slotCount() const;

    /**
     * @brief Set slot count (requires restart)
     */
    void setSlotCount(uint32_t count);

    /**
     * @brief Check if manager is active
     */
    bool isActive() const;

    /**
     * @brief Get the configured subnet
     */
    const IPv6Subnet& subnet() const;

    /**
     * @brief Record usage on current slot
     */
    void recordUsage(uint64_t bytes);

    /**
     * @brief Record usage on a specific slot (id from getNextAddressSlot)
     */
    void recordUsage(uint32_t slotId, uint64_t bytes);

    /**
     * @brief Auto-detect IPv6 prefix from active interface with default gateway
     * @return Detected subnet, or empty subnet if none found
     */
    static IPv6Subnet autoDetectSubnet();

    /**
     * @brief Detect all available IPv6 prefixes on the system
     * @return Vector of detected subnets
     */
    static std::vector<IPv6Subnet> detectAllSubnets();

    /**
     * @brief Rotate a single slot (one client changes its own IP only).
     * @param slotId Slot to rotate
     * @return true if the slot got a new, bound address
     */
    bool rotateSlot(uint32_t slotId);

    /**
     * @brief Mark a slot as unusable after the OS silently overrode its bind.
     *
     * When the outbound local endpoint does not match the requested pool address
     * the slot is NOT actually doing its job - keeping it in rotation makes every
     * subsequent connection egress via the ISP SLAAC address. Deactivate it so
     * the pool converges on addresses the ISP really honours.
     *
     * @return true if a matching slot was found and deactivated
     */
    bool markSlotUnhealthyBind(const std::string& addr);

    /**
     * @brief Remove stale pool addresses left behind by a previous session.
     *
     * Deletes every non-SLAAC/DHCP address that sits inside the CURRENT pool
     * prefix (i.e. addresses this application added on an earlier run and that
     * Windows kept). Addresses of SLAAC/DHCP origin are never touched, and
     * nothing outside the pool prefix is ever touched - so a user's static
     * address in another prefix stays intact.
     *
     * @return number of addresses removed
     */
    uint32_t purgeStaleAddresses();

    /**
     * @brief Bind an IPv6 address to an interface (OS-level)
     */
    bool bindAddress(const IPv6Address& addr, const std::string& iface);

    /**
     * @brief Unbind an IPv6 address from an interface (OS-level)
     */
    bool unbindAddress(const IPv6Address& addr, const std::string& iface);

private:
    IPv6Address generateRandomAddress();

    IPv6Subnet m_subnet;
    std::vector<IPv6Slot> m_slots;
    std::vector<IPv6Address> m_originalAddresses;
    mutable std::mutex m_mutex;
    std::atomic<uint32_t> m_currentIndex{0};
    std::atomic<uint32_t> m_slotCount{50};
    std::atomic<bool> m_active{false};
    std::mt19937_64 m_rng;
};

} // namespace ProxyBridge
