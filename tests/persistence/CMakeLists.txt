# Persistence layer tests
add_executable(persistence_tests
    test_persistence.cpp
)

target_link_libraries(persistence_tests PRIVATE
    persistence
    game
    core
    Catch2::Catch2WithMain
)

add_test(NAME persistence_tests COMMAND persistence_tests)
