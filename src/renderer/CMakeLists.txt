add_library(renderer STATIC
    EmissiveFBO.cpp
    GBuffer.cpp
    GeometryData.cpp
    Material.cpp
    Mesh.cpp
    MeshInstance.cpp
    MeshRenderer.cpp
    Renderable.cpp
    Renderer.cpp
)

target_include_directories(renderer PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(renderer PUBLIC abstract)
