# Network layer
add_library(network STATIC
    network.cpp
)

target_include_directories(network PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(network PRIVATE game)
target_link_libraries(network PUBLIC nlohmann_json::nlohmann_json)