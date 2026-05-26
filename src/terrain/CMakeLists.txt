add_library(terrain STATIC
    CDLODQuadTree.cpp
    TerrainData.cpp
    TerrainPatchMesh.cpp
    TerrainRenderer.cpp
)

target_link_libraries(terrain PUBLIC core abstract)
