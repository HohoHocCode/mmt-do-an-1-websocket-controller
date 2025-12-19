cmake_minimum_required(VERSION 3.20)
project(mmt_ws_controller LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------
# Boost
# ---------------------------------------------------------
find_package(Boost CONFIG REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(unofficial-mysql-connector-c CONFIG REQUIRED)

if(WIN32)
    set(PLATFORM_NETWORK_LIBS ws2_32 mswsock)
    set(PLATFORM_PROCESS_LIBS psapi)
else()
    set(PLATFORM_NETWORK_LIBS)
    set(PLATFORM_PROCESS_LIBS)
endif()

# ---------------------------------------------------------
# OpenCV (from MSYS2 MinGW64)
# ---------------------------------------------------------
find_package(OpenCV CONFIG REQUIRED)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV libs: ${OpenCV_LIBS}")

# ---------------------------------------------------------
# core (header-only)
# ---------------------------------------------------------
add_library(core INTERFACE)

target_include_directories(core INTERFACE 
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(core INTERFACE
    Boost::boost
    Boost::system
)

# ---------------------------------------------------------
# modules (Dispatcher + all system modules)
# ---------------------------------------------------------
add_library(modules
    src/core/dispatcher.cpp                # <<== moved here
    src/modules/process.cpp
    src/modules/screen.cpp
    src/modules/camera.cpp
    src/modules/system_control.cpp
    src/modules/consent.cpp
    src/utils/base64.cpp
)

target_include_directories(modules PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(modules PUBLIC
    opencv_core
    opencv_imgproc
    opencv_highgui
    opencv_videoio
    ${PLATFORM_PROCESS_LIBS}
)

# ---------------------------------------------------------
# network (WsServer, WsClient)
# ---------------------------------------------------------
add_library(network
    src/network/ws_server.cpp
    src/network/ws_client.cpp
)

target_include_directories(network PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(network
    PUBLIC
        core
        Boost::boost
        Boost::system
        ${PLATFORM_NETWORK_LIBS}
)

# ---------------------------------------------------------
# server executable
# ---------------------------------------------------------
add_executable(server 
    src/server/main.cpp
)

target_link_libraries(server 
    PRIVATE 
        core
        network
        modules      # <<== ProcessManager + Dispatcher
        ${PLATFORM_NETWORK_LIBS}
)

# ---------------------------------------------------------
# client executable
# ---------------------------------------------------------
add_executable(client 
    src/client/main.cpp
)

target_link_libraries(client 
    PRIVATE 
        core 
        network
        modules   
        ${PLATFORM_NETWORK_LIBS}
)

# ---------------------------------------------------------
# API library (HTTP + services)
# ---------------------------------------------------------
add_library(api_lib
    src/api/http_server.cpp
    src/api/auth_service.cpp
    src/api/db.cpp
    src/api/password_hash.cpp
    src/api/logger.cpp
    src/api/stream_manager.cpp
    src/api/discovery.cpp
)

target_include_directories(api_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(api_lib
    PUBLIC
        core
        network
        modules
        Boost::boost
        Boost::system
        Threads::Threads
        OpenSSL::Crypto
        OpenSSL::SSL
        unofficial::mysql-connector-c::mysqlclient
        ${PLATFORM_NETWORK_LIBS}
)

# ---------------------------------------------------------
# API executable
# ---------------------------------------------------------
add_executable(mmt_api
    src/api/main.cpp
)

target_link_libraries(mmt_api PRIVATE api_lib)
