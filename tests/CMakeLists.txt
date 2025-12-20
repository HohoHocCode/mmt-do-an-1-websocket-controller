set(TEST_SOURCES
    test_main.cpp
    dispatcher_tests.cpp
    limits_tests.cpp
    path_utils_tests.cpp
)

if (ENABLE_NETWORK)
    list(APPEND TEST_SOURCES ws_smoke_test.cpp)
endif()

add_executable(unit_tests ${TEST_SOURCES})

target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(unit_tests PRIVATE
    core
    modules
    Threads::Threads
)

if (ENABLE_NETWORK)
    target_link_libraries(unit_tests PRIVATE network)
endif()

add_test(NAME unit_tests COMMAND unit_tests)
