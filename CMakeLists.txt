cmake_minimum_required(VERSION 3.20)
project(mmt_ws_controller LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HAVE_DEPS ON)

# ---------------------------------------------------------
# Boost
# ---------------------------------------------------------
find_package(Boost CONFIG COMPONENTS system)
if (NOT Boost_FOUND)
    find_package(Boost COMPONENTS system)
endif()
if (NOT Boost_FOUND)
    set(HAVE_DEPS OFF)
endif()

# ---------------------------------------------------------
# OpenCV (from MSYS2 MinGW64)
# ---------------------------------------------------------
find_package(OpenCV CONFIG)
if (NOT OpenCV_FOUND)
    find_package(OpenCV)
endif()
if (NOT OpenCV_FOUND)
    set(HAVE_DEPS OFF)
endif()

if (NOT HAVE_DEPS)
    message(WARNING "Boost/OpenCV not found; building stub targets for limited functionality.")

    add_library(core INTERFACE)
    target_include_directories(core INTERFACE 
        ${CMAKE_SOURCE_DIR}/include
    )

    add_library(modules
        src/stub/modules_stub.cpp
    )

    target_link_libraries(modules PUBLIC
        core
    )

    add_library(network
        src/stub/network_stub.cpp
    )

    target_link_libraries(network PUBLIC
        core
    )

    add_executable(server 
        src/stub/server_stub.cpp
    )

    target_link_libraries(server 
        PRIVATE 
            core
            network
            modules
    )

    add_executable(client 
        src/stub/client_stub.cpp
    )

    target_link_libraries(client 
        PRIVATE 
            core 
            network
            modules   
    )

    return()
endif()

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
if (WIN32)
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
if (WIN32)
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
if (WIN32)
    target_link_libraries(client PRIVATE ws2_32 mswsock)
endif()
