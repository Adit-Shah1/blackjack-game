# Game layer - depends on core
add_library(game STATIC
    game.cpp
)

target_include_directories(game PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(game PRIVATE core)