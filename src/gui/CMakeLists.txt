# GUI layer - SDL2
add_library(gui STATIC
    gui.cpp
)

target_include_directories(gui PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(gui PRIVATE game audio)

if(SDL2_FOUND)
    target_include_directories(gui PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(gui PRIVATE ${SDL2_LIBRARIES})
endif()

if(SDL2_IMAGE_FOUND)
    target_link_libraries(gui PRIVATE ${SDL2_IMAGE_LIBRARIES})
endif()

if(SDL2_TTF_FOUND)
    target_include_directories(gui PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_link_libraries(gui PRIVATE ${SDL2_TTF_LIBRARIES})
endif()