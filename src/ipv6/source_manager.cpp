#include "ipv6/source_manager.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#endif

namespace ProxyBridge {

SourceManager::SourceManager() = default;
SourceManager::~SourceManager() = default;

bool SourceManager::initialize() { return true; }

uint32_t SourceManager::addSource(NetworkSource source) {
    std::lock_guard<std::mutex> lock(m_mutex);
    source.id = m_nextId++;
    m_sources.push_back(std::move(source));
    return m_sources.back().id;
}

bool SourceManager::removeSource(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::remove_if(m_sources.begin(), m_sources.end(),
        [id](const NetworkSource& s) { return s.id == id; });
    if (it == m_sources.end()) return false;
    m_sources.erase(it, m_sources.end());
    return true;
}

bool SourceManager::updateSource(const NetworkSource& source) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& s : m_sources) {
        if (s.id == source.id) {
            s = source;
            return true;
        }
    }
    return false;
}

std::optional<NetworkSource> SourceManager::getSource(uint32_t id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& s : m_sources) {
        if (s.id == id) return s;
    }
    return std::nullopt;
}

std::vector<NetworkSource> SourceManager::getAllSources() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sources;
}

std::vector<std::string> SourceManager::detectInterfaces() const {
    std::vector<std::string> interfaces;

#ifdef _WIN32
    ULONG bufLen = 15000;
    auto* addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufLen));
    if (!addresses) return interfaces;

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_MULTICAST;
    DWORD result = GetAdaptersAddresses(AF_INET6, flags, nullptr, addresses, &bufLen);

    if (result == ERROR_BUFFER_OVERFLOW) {
        free(addresses);
        addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufLen));
        if (!addresses) return interfaces;
        result = GetAdaptersAddresses(AF_INET6, flags, nullptr, addresses, &bufLen);
    }

    if (result == NO_ERROR) {
        for (auto* adapter = addresses; adapter; adapter = adapter->Next) {
            if (adapter->OperStatus == IfOperStatusUp) {
                // Convert friendly name from wide char
                char name[256];
                WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1,
                                    name, sizeof(name), nullptr, nullptr);
                interfaces.push_back(name);
            }
        }
    }

    free(addresses);
#endif

    return interfaces;
}

std::optional<IPv6Subnet> SourceManager::detectSubnet(const std::string& iface) const {
#ifdef _WIN32
    ULONG bufLen = 15000;
    auto* addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufLen));
    if (!addresses) return std::nullopt;

    DWORD result = GetAdaptersAddresses(AF_INET6, GAA_FLAG_INCLUDE_PREFIX,
                                        nullptr, addresses, &bufLen);
    if (result == ERROR_BUFFER_OVERFLOW) {
        free(addresses);
        addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufLen));
        if (!addresses) return std::nullopt;
        result = GetAdaptersAddresses(AF_INET6, GAA_FLAG_INCLUDE_PREFIX,
                                      nullptr, addresses, &bufLen);
    }

    if (result == NO_ERROR) {
        for (auto* adapter = addresses; adapter; adapter = adapter->Next) {
            char name[256];
            WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1,
                                name, sizeof(name), nullptr, nullptr);

            if (std::string(name) == iface) {
                for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
                    auto* sockAddr = reinterpret_cast<sockaddr_in6*>(unicast->Address.lpSockaddr);
                    if (sockAddr->sin6_family == AF_INET6) {
                        const auto* b = sockAddr->sin6_addr.s6_bytes;
                        // Skip link-local (fe80::/10)
                        if (b[0] == 0xFE && (b[1] & 0xC0) == 0x80) continue;
                        // Skip ULA (fc00::/7)
                        if ((b[0] & 0xFE) == 0xFC) continue;
                        // Skip Teredo (2001::/32) and other non-global ranges:
                        // only accept global unicast 2000::/3
                        if ((b[0] & 0xE0) != 0x20) continue;

                        IPv6Subnet subnet;
                        std::memcpy(subnet.prefix.bytes.data(),
                                    &sockAddr->sin6_addr, 16);
                        subnet.prefixLength = unicast->OnLinkPrefixLength;
                        if (subnet.prefixLength < 1 || subnet.prefixLength > 128)
                            subnet.prefixLength = 64;

                        // Mask off host bits so we store a real network prefix.
                        {
                            int fullBytes = subnet.prefixLength / 8;
                            int remainBits = subnet.prefixLength % 8;
                            for (int i = fullBytes; i < 16; ++i)
                                subnet.prefix.bytes[i] = 0;
                            if (remainBits > 0 && fullBytes < 16) {
                                uint8_t mask = static_cast<uint8_t>(0xFFu << (8 - remainBits));
                                subnet.prefix.bytes[fullBytes] &= mask;
                            }
                        }

                        subnet.ifaceName = iface;

                        free(addresses);
                        return subnet;
                    }
                }
            }
        }
    }

    free(addresses);
#endif
    return std::nullopt;
}

std::optional<NetworkSource> SourceManager::getNextSource() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_sources.empty()) return std::nullopt;

    // Round-robin among enabled sources. Return by value so callers never
    // hold a pointer into the internal vector (which can be reallocated).
    for (size_t attempts = 0; attempts < m_sources.size(); ++attempts) {
        uint32_t idx = m_roundRobin.fetch_add(1) % static_cast<uint32_t>(m_sources.size());
        if (m_sources[idx].enabled) {
            return m_sources[idx];
        }
    }
    return std::nullopt;
}

bool SourceManager::toggleSource(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& s : m_sources) {
        if (s.id == id) { s.enabled = !s.enabled; return s.enabled; }
    }
    return false;
}

size_t SourceManager::count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sources.size();
}

uint32_t SourceManager::totalSlots() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t total = 0;
    for (const auto& s : m_sources) if (s.enabled) total += s.slots;
    return total;
}

bool SourceManager::load(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        nlohmann::json j; file >> j;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_sources.clear();
        uint32_t maxId = 0;

        for (const auto& item : j) {
            NetworkSource src;
            src.id = item.value("id", uint32_t(0));
            src.name = item.value("name", "");
            src.iface = item.value("interface", "");
            src.mode = SourceMode::ModeAutomatic;
            {
                int modeVal = item.value("mode", 0);
                if (modeVal >= 0 && modeVal <= 2) {
                    src.mode = static_cast<SourceMode>(modeVal);
                }
            }
            src.slots = item.value("slots", uint32_t(50));
            if (src.slots == 0 || src.slots > 100000) src.slots = 50;
            src.enabled = item.value("enabled", true);

            if (item.contains("prefix")) {
                auto prefix = IPv6Address::fromString(item["prefix"].get<std::string>());
                if (prefix) {
                    src.subnet.prefix = *prefix;
                    src.subnet.prefixLength = item.value("prefixLength", 64);
                    if (src.subnet.prefixLength < 1 || src.subnet.prefixLength > 128)
                        src.subnet.prefixLength = 64;
                    src.subnet.ifaceName = src.iface;
                }
            }

            if (src.id > maxId) maxId = src.id;
            m_sources.push_back(std::move(src));
        }
        m_nextId = maxId + 1;
        return true;
    } catch (...) { return false; }
}

bool SourceManager::save(const std::string& path) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);
        nlohmann::json arr = nlohmann::json::array();

        for (const auto& src : m_sources) {
            nlohmann::json j;
            j["id"] = src.id;
            j["name"] = src.name;
            j["interface"] = src.iface;
            j["mode"] = static_cast<int>(src.mode);
            j["slots"] = src.slots;
            j["enabled"] = src.enabled;
            j["prefix"] = src.subnet.prefix.toString();
            j["prefixLength"] = src.subnet.prefixLength;
            arr.push_back(j);
        }

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << arr.dump(4);
        return true;
    } catch (...) { return false; }
}

} // namespace ProxyBridge
