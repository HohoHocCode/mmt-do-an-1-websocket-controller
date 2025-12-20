cmake_minimum_required(VERSION 3.20)
project(mmt_ws_controller LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------
# Boost / Network
# ---------------------------------------------------------
option(ENABLE_NETWORK "Build WebSocket networking targets" ON)
find_package(Boost QUIET COMPONENTS system)
if (ENABLE_NETWORK AND NOT Boost_FOUND)
    message(WARNING "Boost not found; disabling network targets.")
    set(ENABLE_NETWORK OFF)
endif()
find_package(Threads REQUIRED)

option(ENABLE_OPENCV "Enable OpenCV dependent modules" ON)
option(BUILD_API "Build API executable" ON)

find_package(OpenSSL QUIET)
find_package(unofficial-mysql-connector-c CONFIG QUIET)

if (BUILD_API AND (NOT OpenSSL_FOUND OR NOT unofficial-mysql-connector-c_FOUND))
    message(WARNING "OpenSSL or MySQL connector not found; disabling API build.")
    set(BUILD_API OFF)
endif()
if (BUILD_API AND NOT ENABLE_NETWORK)
    message(WARNING "Network targets disabled; disabling API build.")
    set(BUILD_API OFF)
endif()

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
find_package(OpenCV QUIET)

if (ENABLE_OPENCV AND OpenCV_FOUND)
    add_compile_definitions(MMT_ENABLE_OPENCV)
elseif(ENABLE_OPENCV)
    message(WARNING "OpenCV not found; disabling OpenCV-dependent modules.")
    set(ENABLE_OPENCV OFF)
endif()

if (OpenCV_FOUND)
    message(STATUS "OpenCV version: ${OpenCV_VERSION}")
    message(STATUS "OpenCV include: ${OpenCV_INCLUDE_DIRS}")
    message(STATUS "OpenCV libs: ${OpenCV_LIBS}")
endif()

# ---------------------------------------------------------
# core (header-only)
# ---------------------------------------------------------
add_library(core INTERFACE)

target_include_directories(core INTERFACE 
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(core INTERFACE
)

if (Boost_FOUND)
    target_link_libraries(core INTERFACE
        Boost::boost
        Boost::system
    )
endif()

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
    src/utils/path_utils.cpp
)

target_include_directories(modules PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(modules PUBLIC
    ${PLATFORM_PROCESS_LIBS}
)

if (OpenCV_FOUND AND ENABLE_OPENCV)
    target_include_directories(modules PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(modules PUBLIC
        opencv_core
        opencv_imgproc
        opencv_highgui
        opencv_videoio
    )
endif()

# ---------------------------------------------------------
# network (WsServer, WsClient)
# ---------------------------------------------------------
if (ENABLE_NETWORK)
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
endif()

# ---------------------------------------------------------
# server executable
# ---------------------------------------------------------
if (ENABLE_NETWORK)
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
endif()

# ---------------------------------------------------------
# client executable
# ---------------------------------------------------------
if (ENABLE_NETWORK)
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
endif()

# ---------------------------------------------------------
# API library (HTTP + services)
# ---------------------------------------------------------
if (BUILD_API)
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
endif()

include(CTest)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif()
