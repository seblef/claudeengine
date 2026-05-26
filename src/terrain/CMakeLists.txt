add_library(terrain STATIC
    TerrainData.cpp
)

target_link_libraries(terrain PUBLIC core abstract)
