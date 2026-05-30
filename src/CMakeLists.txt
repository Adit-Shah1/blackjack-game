# Main executable
add_executable(blackjack_game 
    main.cpp
)

# Include public headers
target_include_directories(blackjack_game PUBLIC 
    ${PROJECT_SOURCE_DIR}/include
)

# Link SDL2 libraries via pkg-config variables
if(SDL2_FOUND)
    target_include_directories(blackjack_game PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(blackjack_game PRIVATE ${SDL2_LIBRARIES} -L${SDL2_LIBRARY_DIRS})
endif()

if(SDL2_IMAGE_FOUND)
    target_link_libraries(blackjack_game PRIVATE ${SDL2_IMAGE_LIBRARIES})
endif()

if(SDL2_TTF_FOUND)
    target_link_libraries(blackjack_game PRIVATE ${SDL2_TTF_LIBRARIES})
endif()

if(SDL2_MIXER_FOUND)
    target_link_libraries(blackjack_game PRIVATE ${SDL2_MIXER_LIBRARIES})
endif()

# Link nlohmann_json
target_link_libraries(blackjack_game PRIVATE nlohmann_json::nlohmann_json)

# Set output name
set_target_properties(blackjack_game PROPERTIES
    OUTPUT_NAME "blackjack"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)