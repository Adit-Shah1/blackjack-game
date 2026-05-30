# AI layer tests
add_executable(ai_tests
    test_strategy.cpp
)

target_link_libraries(ai_tests PRIVATE
    ai
    game
    core
    Catch2::Catch2WithMain
)

add_test(NAME ai_tests COMMAND ai_tests)