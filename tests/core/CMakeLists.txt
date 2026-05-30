# Core layer tests
add_executable(core_tests
    test_card.cpp
    test_hand.cpp
    test_shoe.cpp
)

target_link_libraries(core_tests PRIVATE
    core
    Catch2::Catch2WithMain
)

# Register tests directly with CTest
add_test(NAME core_tests COMMAND core_tests)