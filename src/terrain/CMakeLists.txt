add_library(terrain STATIC
    CDLODQuadTree.cpp
    TerrainData.cpp
    TerrainPatchMesh.cpp
)

target_link_libraries(terrain PUBLIC core abstract)
