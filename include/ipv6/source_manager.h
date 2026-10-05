#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>
#include <optional>
#include <atomic>
#include "ipv6/ipv6_manager.h"

// Qt/Windows headers re-define 'interface' as 'struct' via objbase.h
// This must be undone here since Qt headers may be included before us
#ifdef interface
#undef interface
#endif

namespace ProxyBridge {

enum class SourceMode : uint8_t {
    ModeAutomatic = 0,
    ModeManual = 1,
    ModeRandom = 2
};

struct NetworkSource {
    uint32_t id;
    std::string name;
    std::string iface;
    IPv6Subnet subnet;
    SourceMode mode;
    uint32_t slots;
    bool enabled;
    uint64_t totalRequests;
    uint64_t totalBytes;

    NetworkSource()
        : id(0), mode(SourceMode::ModeAutomatic), slots(50),
          enabled(true), totalRequests(0), totalBytes(0) {}
};

class SourceManager {
public:
    SourceManager();
    ~SourceManager();

    bool initialize();
    uint32_t addSource(NetworkSource source);
    bool removeSource(uint32_t id);
    bool updateSource(const NetworkSource& source);
    std::optional<NetworkSource> getSource(uint32_t id) const;
    std::vector<NetworkSource> getAllSources() const;
    std::vector<std::string> detectInterfaces() const;
    std::optional<IPv6Subnet> detectSubnet(const std::string& iface) const;
    std::optional<NetworkSource> getNextSource();
    bool toggleSource(uint32_t id);
    size_t count() const;
    uint32_t totalSlots() const;
    bool load(const std::string& path);
    bool save(const std::string& path);

private:
    std::vector<NetworkSource> m_sources;
    mutable std::mutex m_mutex;
    std::atomic<uint32_t> m_nextId{1};
    std::atomic<uint32_t> m_roundRobin{0};
};

} // namespace ProxyBridge
