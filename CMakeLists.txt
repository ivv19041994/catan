cmake_minimum_required(VERSION 3.20)

project(catan_engine VERSION 0.1.0 LANGUAGES CXX)

option(CATAN_BUILD_CONSOLE "Build the experimental console application" ON)
option(CATAN_BUILD_TESTS "Build unit tests" ON)

set(CMAKE_CXX_EXTENSIONS OFF)

add_library(catan_engine
    src/catan.cpp
    src/dice.cpp
    src/exception.cpp
    src/facet.cpp
    src/game_controller.cpp
    src/gex.cpp
    src/map.cpp
    src/node.cpp
    src/player.cpp
)

target_include_directories(catan_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(catan_engine PUBLIC cxx_std_20)

add_library(catan_render
    src/map_rander.cpp
    src/player_randerer.cpp
    src/svg.cpp
)

target_link_libraries(catan_render PUBLIC catan_engine)
target_include_directories(catan_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(catan_render PUBLIC cxx_std_20)

if(CATAN_BUILD_CONSOLE)
    add_executable(catan
        src/console_interface.cpp
        src/main.cpp
    )
    target_link_libraries(catan PRIVATE catan_render)
    target_compile_features(catan PRIVATE cxx_std_20)
endif()

if(CATAN_BUILD_TESTS)
    include(CTest)

    if(BUILD_TESTING)
        add_executable(catan_engine_tests tests/game_controller_tests.cpp)
        target_link_libraries(catan_engine_tests PRIVATE catan_engine)
        target_compile_features(catan_engine_tests PRIVATE cxx_std_20)
        add_test(NAME catan_engine_tests COMMAND catan_engine_tests)
    endif()
endif()
