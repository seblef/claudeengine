add_library(renderer STATIC
    Renderer.cpp
)

target_include_directories(renderer PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(renderer PUBLIC abstract)
