cmake_minimum_required(VERSION 3.20)
project(ProxyBridgePro VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX- /MP /permissive- /Zc:__cplusplus)
    add_compile_definitions(_WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /GL /Gy")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
endif()

# Find packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Network)
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Core library
add_library(ProxyBridgeCore STATIC
    src/core/application.cpp
    src/core/thread_pool.cpp
    src/core/event_bus.cpp
    src/config/config_manager.cpp
    src/log/log_manager.cpp
    src/network/async_socket.cpp
    src/network/connection_manager.cpp
    src/network/ssl_context.cpp
    src/ipv6/ipv6_manager.cpp
    src/ipv6/ipv6_rotator.cpp
    src/ipv6/source_manager.cpp
    src/ipv6/subnet_pool.cpp
    src/proxy/proxy_server.cpp
    src/proxy/backconnect_proxy.cpp
    src/proxy/http_handler.cpp
    src/proxy/socks5_handler.cpp
    src/proxy/auth_manager.cpp
    src/frp/frp_client.cpp
    src/frp/frp_config.cpp
    src/api/api_server.cpp
    src/api/api_routes.cpp
)

target_link_libraries(ProxyBridgeCore PUBLIC
    Qt6::Core
    Qt6::Network
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

if(WIN32)
    target_link_libraries(ProxyBridgeCore PUBLIC ws2_32 wsock32 iphlpapi)
endif()

# GUI library
add_library(ProxyBridgeGui STATIC
    src/gui/main_window.cpp
    src/gui/sidebar.cpp
    src/gui/proxy_page.cpp
    src/gui/advance_page.cpp
    src/gui/frp_page.cpp
    src/gui/sources_page.cpp
    src/gui/license_page.cpp
    src/gui/theme_manager.cpp
    src/gui/status_panel.cpp
    src/gui/log_panel.cpp
    include/gui/main_window.h
    include/gui/sidebar.h
    include/gui/proxy_page.h
    include/gui/advance_page.h
    include/gui/frp_page.h
    include/gui/sources_page.h
    include/gui/license_page.h
    include/gui/theme_manager.h
    include/gui/status_panel.h
    include/gui/log_panel.h
)

target_link_libraries(ProxyBridgeGui PUBLIC
    ProxyBridgeCore
    Qt6::Widgets
    Qt6::Gui
)

# Main executable
add_executable(pb_headless tests/headless_main.cpp)
    target_link_libraries(pb_headless PRIVATE ProxyBridgeCore)
    target_include_directories(pb_headless PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(ProxyBridgePro WIN32
    src/main.cpp
    resources/resources.qrc
)

target_link_libraries(ProxyBridgePro PRIVATE
    ProxyBridgeGui
)

install(TARGETS ProxyBridgePro RUNTIME DESTINATION bin)
