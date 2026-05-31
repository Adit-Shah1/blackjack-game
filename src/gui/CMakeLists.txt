# GUI layer - SDL2
add_library(gui STATIC
    gui_helpers.cpp
    gui_widgets.cpp
    gui_core.cpp
    gui_menus.cpp
    gui_game.cpp
    gui_network.cpp
    gui_screens.cpp
)

target_include_directories(gui PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(gui PRIVATE game audio network nlohmann_json::nlohmann_json)

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