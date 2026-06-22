add_library(game STATIC
    FPSCameraController.cpp
    PlayerVehicleController.cpp
    GameCamera.cpp
    GameObject.cpp
    GameLight.cpp
    GameMaterial.cpp
    GameMesh.cpp
    GameParticleSystem.cpp
    GamePivot.cpp
    GamePlayerStart.cpp
    GameSoundEmitter.cpp
    SoundEffectComponent.cpp
    GameSystem.cpp
    GameTerrain.cpp
    MapLoader.cpp
    MeshTemplate.cpp
)

target_include_directories(game PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(game PUBLIC core abstract renderer terrain environment particles physics audio ui)
