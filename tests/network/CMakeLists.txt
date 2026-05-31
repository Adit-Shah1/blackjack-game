# Network layer tests
add_executable(network_tests
    test_network.cpp
)

target_link_libraries(network_tests PRIVATE
    network
    game
    core
    ai
    Catch2::Catch2WithMain
    nlohmann_json::nlohmann_json
)

add_test(NAME network_tests COMMAND network_tests)
