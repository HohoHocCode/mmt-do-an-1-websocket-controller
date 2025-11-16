cmake_minimum_required(VERSION 3.16)
project(WebSocketController)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

add_subdirectory(src)
