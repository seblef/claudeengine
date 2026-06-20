add_library(ui STATIC
    FontAtlas.cpp
)

target_include_directories(ui PUBLIC  "${CMAKE_SOURCE_DIR}/src"
                               PRIVATE ${stb_SOURCE_DIR})

target_link_libraries(ui PUBLIC abstract)
