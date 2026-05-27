add_library(terrain STATIC
    CDLODQuadTree.cpp
    FoliageLayer.cpp
    TerrainData.cpp
    TerrainMaterial.cpp
    TerrainNormalMap.cpp
    TerrainPatchMesh.cpp
    TerrainRenderer.cpp
)

target_link_libraries(terrain PUBLIC core abstract)
