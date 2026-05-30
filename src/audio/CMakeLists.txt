# Audio layer
add_library(audio STATIC
    audio.cpp
)

target_include_directories(audio PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(audio PRIVATE game)

if(SDL2_MIXER_FOUND)
    target_include_directories(audio PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
    target_link_libraries(audio PRIVATE ${SDL2_MIXER_LIBRARIES})
endif()