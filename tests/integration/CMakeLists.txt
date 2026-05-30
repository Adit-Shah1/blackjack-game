# Integration tests
add_executable(integration_tests
    test_full_round.cpp
)

target_link_libraries(integration_tests PRIVATE
    game
    ai
    core
    Catch2::Catch2WithMain
)

add_test(NAME integration_tests COMMAND integration_tests)