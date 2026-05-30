# Game layer tests
add_executable(game_tests
    test_round.cpp
)

target_link_libraries(game_tests PRIVATE
    game
    core
    Catch2::Catch2WithMain
)

add_test(NAME game_tests COMMAND game_tests)