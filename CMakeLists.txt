cmake_minimum_required(VERSION 3.20)
project(mmt_ws_controller LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MMT_CODEX_SANDBOX "Build minimal sandbox targets for Codex" OFF)

# ---------------------------------------------------------
# Boost
# ---------------------------------------------------------
find_package(Boost CONFIG QUIET COMPONENTS system)

# ---------------------------------------------------------
# OpenCV (from MSYS2 MinGW64)
# ---------------------------------------------------------
find_package(OpenCV CONFIG QUIET)

set(MMT_SANDBOX_REASONS "")
if(MMT_CODEX_SANDBOX)
    list(APPEND MMT_SANDBOX_REASONS "MMT_CODEX_SANDBOX option is ON")
endif()

if(NOT Boost_FOUND)
    list(APPEND MMT_SANDBOX_REASONS "Boost::system not found")
endif()

if(NOT OpenCV_FOUND)
    list(APPEND MMT_SANDBOX_REASONS "OpenCV not found")
endif()

if(NOT WIN32)
    list(APPEND MMT_SANDBOX_REASONS "Non-Windows host (full build targets use Windows APIs)")
endif()

set(MMT_ENABLE_FULL_BUILD ON)
if(MMT_SANDBOX_REASONS)
    set(MMT_ENABLE_FULL_BUILD OFF)
endif()

if(MMT_ENABLE_FULL_BUILD)
    message(STATUS "Full feature build enabled")
    if(Boost_FOUND)
        message(STATUS "Boost version: ${Boost_VERSION}")
    endif()
    if(OpenCV_FOUND)
        message(STATUS "OpenCV version: ${OpenCV_VERSION}")
        message(STATUS "OpenCV include: ${OpenCV_INCLUDE_DIRS}")
        message(STATUS "OpenCV libs: ${OpenCV_LIBS}")
    endif()
else()
    message(STATUS "Sandbox build enabled; skipping full targets due to: ${MMT_SANDBOX_REASONS}")
endif()

if(NOT MMT_ENABLE_FULL_BUILD)
    add_executable(codex_sandbox
        src/codex_sandbox.cpp
    )
    target_compile_features(codex_sandbox PRIVATE cxx_std_20)
    return()
endif()

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
)

if(WIN32)
    target_link_libraries(network PUBLIC ws2_32 mswsock)
endif()

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
)

if(WIN32)
    target_link_libraries(server PRIVATE ws2_32 mswsock)
endif()

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
)

if(WIN32)
    target_link_libraries(client PRIVATE ws2_32 mswsock)
endif()
