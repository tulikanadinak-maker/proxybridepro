#include "network/ssl_context.h"

namespace ProxyBridge {

SslContextManager::SslContextManager() = default;
SslContextManager::~SslContextManager() = default;

bool SslContextManager::initialize() {
    try {
        m_clientContext = std::make_unique<ssl::context>(ssl::context::tlsv12_client);
        m_clientContext->set_default_verify_paths();
        // Verify peer certificates on the client (outbound TLS) side.
        m_clientContext->set_verify_mode(ssl::verify_peer);
        m_clientContext->set_verify_callback([](bool preverified, ssl::verify_context& ctx) {
            // Minimal verification: rely on the default chain verification via
            // set_default_verify_paths(); accept only preverified chains here.
            (void)ctx;
            return preverified;
        });

        m_serverContext = std::make_unique<ssl::context>(ssl::context::tlsv12_server);
        m_serverContext->set_options(ssl::context::default_workarounds |
                                     ssl::context::no_sslv2 | ssl::context::no_sslv3);
        return true;
    } catch (...) { return false; }
}

ssl::context& SslContextManager::clientContext() { return *m_clientContext; }
ssl::context& SslContextManager::serverContext() { return *m_serverContext; }

} // namespace ProxyBridge
