# Core library - SDL-free blackjack engine
add_library(core STATIC
    card.cpp
    hand.cpp
    shoe.cpp
    rules.cpp
)

target_include_directories(core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)