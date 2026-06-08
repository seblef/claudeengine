add_library(terrain STATIC
    stb_perlin_impl.cpp
    CDLODQuadTree.cpp
    FoliageLayer.cpp
    TerrainData.cpp
    TerrainGenerator.cpp
    TerrainMaterial.cpp
    TerrainPatchMesh.cpp
    TerrainRenderer.cpp
)

target_include_directories(terrain PRIVATE ${stb_SOURCE_DIR})

target_link_libraries(terrain PUBLIC core abstract)
