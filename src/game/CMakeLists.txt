add_library(game STATIC
    GameObject.cpp
    GameLight.cpp
    MeshTemplate.cpp
)

target_include_directories(game PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(game PUBLIC core abstract renderer)
