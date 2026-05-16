add_library(renderer STATIC
    CircleSpotLight.cpp
    EmissiveFBO.cpp
    GBuffer.cpp
    GeometryData.cpp
    GeometryUtils.cpp
    GlobalLight.cpp
    Light.cpp
    LightRenderer.cpp
    Material.cpp
    Mesh.cpp
    MeshInstance.cpp
    MeshLoader.cpp
    MeshRenderer.cpp
    NoCullingVisibilitySystem.cpp
    OctreeVisibilitySystem.cpp
    OmniLight.cpp
    RectangleSpotLight.cpp
    Renderable.cpp
    Renderer.cpp
    ShadowCubeMap.cpp
    ShadowDebugRenderer.cpp
    ShadowMap.cpp
    ShadowMapPool.cpp
    ShadowRenderer.cpp
)

target_include_directories(renderer PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(renderer PUBLIC abstract PRIVATE mesh)
