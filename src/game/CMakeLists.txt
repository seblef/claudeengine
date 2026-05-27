add_library(game STATIC
    FPSCameraController.cpp
    GameCamera.cpp
    GameObject.cpp
    GameLight.cpp
    GameMaterial.cpp
    GameMesh.cpp
    GamePlayerStart.cpp
    GameSystem.cpp
    GameTerrain.cpp
    MapLoader.cpp
    MeshTemplate.cpp
)

target_include_directories(game PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(game PUBLIC core abstract renderer terrain)
