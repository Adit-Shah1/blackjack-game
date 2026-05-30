# Persistence layer
add_library(persistence STATIC
    persistence.cpp
)

target_include_directories(persistence PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(persistence PUBLIC core game)
target_link_libraries(persistence PUBLIC nlohmann_json::nlohmann_json)