# Platform layer
add_library(platform STATIC
    platform.cpp
)

target_include_directories(platform PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)