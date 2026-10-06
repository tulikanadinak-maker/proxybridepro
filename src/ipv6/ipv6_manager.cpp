#include "ipv6/ipv6_manager.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <optional>
#include <cstdio>
#include <array>

#ifndef _WIN32
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

namespace ProxyBridge {

// IPv6Address implementation
std::string IPv6Address::toString() const {
    char buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, bytes.data(), buf, sizeof(buf));
    return std::string(buf);
}

std::optional<IPv6Address> IPv6Address::fromString(const std::string& str) {
    IPv6Address addr;
    if (inet_pton(AF_INET6, str.c_str(), addr.bytes.data()) != 1) {
        return std::nullopt;
    }
    return addr;
}

IPv6Address IPv6Address::random(const IPv6Address& prefix, int prefixLen) {
    IPv6Address addr = prefix;

    // Validate prefix length: must describe at least one host bit.
    if (prefixLen < 1) prefixLen = 1;
    if (prefixLen > 128) prefixLen = 128;

    // Thread-local RNG - avoids re-seeding (and std::random_device cost)
    // on every call, which also guarantees good statistical quality.
    static thread_local std::mt19937_64 rng([] {
        std::random_device rd;
        std::seed_seq seed{rd(), rd(), rd(), rd(),
                           static_cast<unsigned>(std::chrono::steady_clock::now()
                               .time_since_epoch().count())};
        return std::mt19937_64(seed);
    }());

    // Fill bytes after the prefix with random data
    int fullBytes = prefixLen / 8;
    int remainBits = prefixLen % 8;

    if (fullBytes >= 16) {
        // /128: no host bits available - nothing to randomise.
        return addr;
    }

    if (remainBits > 0) {
        uint8_t mask = static_cast<uint8_t>(0xFFu << (8 - remainBits));
        uint8_t randomByte = static_cast<uint8_t>(rng() & 0xFF);
        addr.bytes[fullBytes] = static_cast<uint8_t>((prefix.bytes[fullBytes] & mask) |
                                                     (randomByte & ~mask));
        fullBytes++;
    }

    for (int i = fullBytes; i < 16; ++i) {
        addr.bytes[i] = static_cast<uint8_t>(rng() & 0xFF);
    }

    // Avoid the all-zero host (subnet-router anycast address) which is not
    // assignable to a host. Force the interface identifier to be non-zero.
    int hostStart = prefixLen / 8;
    bool zero = true;
    for (int i = hostStart; i < 16; ++i) {
        if (addr.bytes[i] != 0) { zero = false; break; }
    }
    if (zero) {
        addr.bytes[15] = 1;  // never leave the interface id all-zero
    }

    return addr;
}

bool IPv6Address::operator==(const IPv6Address& other) const {
    return bytes == other.bytes;
}

// IPv6Manager implementation
IPv6Manager::IPv6Manager() = default;
IPv6Manager::~IPv6Manager() { stop(); }

bool IPv6Manager::initialize(const IPv6Subnet& subnet, uint32_t slotCount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_subnet = subnet;
    m_slotCount = slotCount;
    return true;
}

bool IPv6Manager::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_active) return true;

    m_slots.clear();
    m_originalAddresses.clear();

    uint32_t count = m_slotCount.load();
    for (uint32_t i = 0; i < count; ++i) {
        IPv6Address addr = generateRandomAddress();

        if (bindAddress(addr, m_subnet.ifaceName)) {
            IPv6Slot slot;
            slot.id = i;
            slot.address = addr;
            slot.ifaceName = m_subnet.ifaceName;
            slot.active = true;
            slot.createdAt = std::chrono::steady_clock::now();
            m_slots.push_back(slot);
            m_originalAddresses.push_back(addr);
        }
    }

    m_active = !m_slots.empty();
    return m_active;
}

void IPv6Manager::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_active) return;

    for (auto& slot : m_slots) {
        if (slot.active) {
            unbindAddress(slot.address, slot.ifaceName);
            slot.active = false;
        }
    }
    m_slots.clear();
    m_active = false;
}

void IPv6Manager::rotateAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& slot : m_slots) {
        if (!slot.active) continue;

        IPv6Address newAddr = generateRandomAddress();
        if (bindAddress(newAddr, slot.ifaceName)) {
            // Only drop the old address once the new one is really bound -
            // avoids leaving the slot without any working address.
            unbindAddress(slot.address, slot.ifaceName);
            slot.address = newAddr;
            slot.active = true;
            slot.requestCount = 0;
            slot.bytesTransferred = 0;
            slot.createdAt = std::chrono::steady_clock::now();
        } else {
            // Bind failed: keep the old address and leave the slot active so
            // we do not create a zombie/unusable slot.
            slot.active = false;
            unbindAddress(slot.address, slot.ifaceName);
        }
    }
}

void IPv6Manager::resetAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (size_t i = 0; i < m_slots.size() && i < m_originalAddresses.size(); ++i) {
        if (!m_slots[i].active) continue;

        IPv6Address target = m_originalAddresses[i];
        if (bindAddress(target, m_slots[i].ifaceName)) {
            unbindAddress(m_slots[i].address, m_slots[i].ifaceName);
            m_slots[i].address = target;
            m_slots[i].active = true;
            m_slots[i].requestCount = 0;
        } else {
            m_slots[i].active = false;
        }
    }
}

std::optional<std::pair<uint32_t, std::string>> IPv6Manager::getNextAddressSlot() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_slots.empty()) return std::nullopt;

    // Skip inactive slots - round-robin only among working addresses.
    for (size_t attempts = 0; attempts < m_slots.size(); ++attempts) {
        uint32_t index = m_currentIndex.fetch_add(1) % static_cast<uint32_t>(m_slots.size());
        auto& slot = m_slots[index];
        if (!slot.active) continue;
        slot.requestCount++;
        return std::make_pair(slot.id, slot.address.toString());
    }
    return std::nullopt;
}

std::string IPv6Manager::getNextAddress() {
    auto slot = getNextAddressSlot();
    if (!slot) return "";
    return slot->second;
}

std::vector<IPv6Slot> IPv6Manager::getActiveSlots() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_slots;
}

uint32_t IPv6Manager::slotCount() const { return m_slotCount; }
void IPv6Manager::setSlotCount(uint32_t count) { m_slotCount = count; }
bool IPv6Manager::isActive() const { return m_active; }
const IPv6Subnet& IPv6Manager::subnet() const { return m_subnet; }

void IPv6Manager::recordUsage(uint64_t bytes) {
    // Attribute to the last handed-out slot (legacy path, kept for
    // compatibility when the caller has no slot id).
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_slots.empty()) return;
    uint32_t index = (m_currentIndex.load() + m_slots.size() - 1) %
                     static_cast<uint32_t>(m_slots.size());
    m_slots[index].bytesTransferred += bytes;
}

void IPv6Manager::recordUsage(uint32_t slotId, uint64_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& slot : m_slots) {
        if (slot.id == slotId) {
            slot.bytesTransferred += bytes;
            return;
        }
    }
}

IPv6Address IPv6Manager::generateRandomAddress() {
    return IPv6Address::random(m_subnet.prefix, m_subnet.prefixLength);
}

namespace {

// Only allow characters that are safe inside a shell-less command line for
// interface names / addresses (letters, digits, '.', ':', '-', '_', '%').
bool isValidIfaceToken(const std::string& s) {
    if (s.empty() || s.size() > 64) return false;
    for (unsigned char c : s) {
        if (!std::isalnum(c) && c != '.' && c != ':' && c != '-' && c != '_' && c != '%') {
            return false;
        }
    }
    return true;
}

bool isValidIpv6Token(const std::string& s) {
    IPv6Address parsed;
    return inet_pton(AF_INET6, s.c_str(), parsed.bytes.data()) == 1;
}

#ifndef _WIN32
// Run a command with exec-style argv (no shell), return exit code 0 on success.
bool runCommand(const std::vector<std::string>& argv) {
    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
    cargv.push_back(nullptr);

    pid_t pid = 0;
    if (posix_spawn(&pid, cargv[0], nullptr, nullptr, cargv.data(), environ) != 0) {
        return false;
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
#endif

} // anonymous namespace

bool IPv6Manager::bindAddress(const IPv6Address& addr, const std::string& iface) {
#ifdef _WIN32
    // Use netsh to add IPv6 address to interface
    std::string cmd = std::string("netsh interface ipv6 add address \"") + iface +
                      "\" " + addr.toString() + " type=unicast validlifetime=infinite preferredlifetime=infinite store=active";

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                       nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                       nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0;
    }
    return false;
#else
    // Linux: add address via `ip -6 addr add <addr>/<plen> dev <iface>`
    if (!isValidIpv6Token(addr.toString()) || !isValidIfaceToken(iface)) return false;
    std::string cidr = addr.toString() + "/" + std::to_string(m_subnet.prefixLength);
    return runCommand({"ip", "-6", "addr", "add", cidr, "dev", iface});
#endif
}

bool IPv6Manager::unbindAddress(const IPv6Address& addr, const std::string& iface) {
#ifdef _WIN32
    std::string cmd = std::string("netsh interface ipv6 delete address \"") + iface +
                      "\" " + addr.toString();

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                       nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                       nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0;
    }
    return false;
#else
    if (!isValidIpv6Token(addr.toString()) || !isValidIfaceToken(iface)) return false;
    std::string cidr = addr.toString() + "/" + std::to_string(m_subnet.prefixLength);
    return runCommand({"ip", "-6", "addr", "del", cidr, "dev", iface});
#endif
}

IPv6Subnet IPv6Manager::autoDetectSubnet() {
    auto all = detectAllSubnets();
    if (!all.empty()) return all[0];
    return IPv6Subnet{};
}

std::vector<IPv6Subnet> IPv6Manager::detectAllSubnets() {
    std::vector<IPv6Subnet> results;

#ifdef _WIN32
    ULONG bufLen = 15000;
    auto* addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufLen));
    if (!addresses) return results;

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS;
    DWORD result = GetAdaptersAddresses(AF_INET6, flags, nullptr, addresses, &bufLen);

    if (result == ERROR_BUFFER_OVERFLOW) {
        free(addresses);
        addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufLen));
        if (!addresses) return results;
        result = GetAdaptersAddresses(AF_INET6, flags, nullptr, addresses, &bufLen);
    }

    if (result == NO_ERROR) {
        for (auto* adapter = addresses; adapter; adapter = adapter->Next) {
            if (adapter->OperStatus != IfOperStatusUp) continue;

            // Get interface name
            char ifName[256];
            WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1,
                                ifName, sizeof(ifName), nullptr, nullptr);

            // Check if this adapter has a default gateway (IPv6)
            std::string gateway;
            for (auto* gw = adapter->FirstGatewayAddress; gw; gw = gw->Next) {
                auto* sa6 = reinterpret_cast<sockaddr_in6*>(gw->Address.lpSockaddr);
                if (sa6->sin6_family == AF_INET6) {
                    char gwBuf[INET6_ADDRSTRLEN];
                    inet_ntop(AF_INET6, &sa6->sin6_addr, gwBuf, sizeof(gwBuf));
                    gateway = gwBuf;
                    break;
                }
            }

            // Enumerate unicast addresses
            for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
                auto* sa6 = reinterpret_cast<sockaddr_in6*>(unicast->Address.lpSockaddr);
                if (sa6->sin6_family != AF_INET6) continue;

                // Skip link-local (fe80::)
                if (sa6->sin6_addr.s6_bytes[0] == 0xFE &&
                    sa6->sin6_addr.s6_bytes[1] == 0x80) continue;

                // Skip ULA (fd::)
                if (sa6->sin6_addr.s6_bytes[0] == 0xFD) continue;

                int prefixLen = unicast->OnLinkPrefixLength;
                if (prefixLen != 64) {
                    // Windows sometimes reports a non-64 OnLinkPrefixLength for
                    // SLAAC/temporary addresses even though the on-link subnet
                    // is /64. Derive the /64 prefix from the address itself so
                    // a changed ISP delegation is still detected.
                    if (prefixLen < 1 || prefixLen > 64) prefixLen = 64;
                }

                // Extract /64 prefix (zero out host portion - last 8 bytes)
                IPv6Subnet subnet;
                std::memcpy(subnet.prefix.bytes.data(), &sa6->sin6_addr, 16);
                for (int i = 8; i < 16; i++) subnet.prefix.bytes[i] = 0;

                subnet.prefixLength = 64;
                subnet.ifaceName = ifName;
                subnet.gateway = gateway;

                // Avoid duplicates
                bool duplicate = false;
                for (const auto& existing : results) {
                    if (existing.prefix == subnet.prefix) { duplicate = true; break; }
                }
                if (!duplicate) {
                    results.push_back(subnet);
                }
            }
        }
    }

    free(addresses);
#endif

    return results;
}

} // namespace ProxyBridge
