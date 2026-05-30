# AI layer
add_library(ai STATIC
    ai.cpp
)

target_include_directories(ai PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(ai PRIVATE core game)