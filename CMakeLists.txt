cmake_minimum_required(VERSION 3.20)

project(catan_engine VERSION 0.1.0 LANGUAGES CXX)

option(CATAN_BUILD_CONSOLE "Build the experimental console application" ON)
option(CATAN_BUILD_TESTS "Build unit tests" ON)
option(CATAN_BUILD_DEDICATED_SERVER "Build the portable dedicated server" ON)

set(CMAKE_CXX_EXTENSIONS OFF)

add_library(catan_engine
    src/catan.cpp
    src/dice.cpp
    src/development_card_deck.cpp
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

if(CATAN_BUILD_DEDICATED_SERVER)
    add_library(catan_dedicated
        src/dedicated_protocol.cpp
        src/dedicated_server.cpp
    )
    target_link_libraries(catan_dedicated PUBLIC catan_engine)
    target_include_directories(catan_dedicated PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_features(catan_dedicated PUBLIC cxx_std_20)

    add_executable(catan-dedicated-server src/dedicated_server_main.cpp)
    target_link_libraries(catan-dedicated-server PRIVATE catan_dedicated)

    add_executable(catan-dedicated-probe src/dedicated_probe_main.cpp)
    target_compile_features(catan-dedicated-probe PRIVATE cxx_std_20)
endif()

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
        function(add_catan_domain_test domain)
            set(target "catan_${domain}_tests")
            add_executable(${target} "tests/${domain}/${domain}_tests.cpp")
            target_link_libraries(${target} PRIVATE catan_engine)
            target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/support)
            target_compile_features(${target} PRIVATE cxx_std_20)
            add_test(NAME ${target} COMMAND ${target})
        endfunction()

        add_catan_domain_test(setup)
        add_catan_domain_test(production)
        add_catan_domain_test(construction)
        add_catan_domain_test(trade)
        add_catan_domain_test(robber)
        add_catan_domain_test(development_cards)
        add_catan_domain_test(awards_victory)
        add_catan_domain_test(map)
        add_catan_domain_test(player_deck)

        if(CATAN_BUILD_DEDICATED_SERVER)
            add_executable(catan_dedicated_server_tests
                tests/dedicated_server/dedicated_server_tests.cpp
            )
            target_link_libraries(catan_dedicated_server_tests PRIVATE catan_dedicated)
            target_include_directories(catan_dedicated_server_tests PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/support)
            target_compile_features(catan_dedicated_server_tests PRIVATE cxx_std_20)
            add_test(NAME catan_dedicated_server_tests COMMAND catan_dedicated_server_tests)
        endif()
    endif()
endif()
