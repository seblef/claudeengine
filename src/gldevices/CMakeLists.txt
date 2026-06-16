add_library(gldevices STATIC
    GLConstantBuffer.cpp
    GLDevices.cpp
    GLGeometryBuffer.cpp
    GLIndexBuffer.cpp
    GLNormalMapTexture.cpp
    GLRawTexture.cpp
    GLTileableTexture.cpp
    GLRenderTarget.cpp
    GLRenderTargetCube.cpp
    GLRenderTargetGroup.cpp
    GLShader.cpp
    GLShaderStorageBuffer.cpp
    GLTexture.cpp
    GLVideoDevice.cpp
    GLVertexBuffer.cpp
    stb_image_impl.cpp
)

target_include_directories(gldevices PUBLIC "${CMAKE_SOURCE_DIR}/src"
                                     PRIVATE ${stb_SOURCE_DIR}
                                             ${gli_SOURCE_DIR}
                                             ${glm_SOURCE_DIR})

find_package(OpenGL REQUIRED)
target_link_libraries(gldevices PUBLIC abstract PRIVATE glfw OpenGL::GL)

if(TIFF_FOUND)
    target_link_libraries(gldevices PRIVATE TIFF::TIFF)
endif()
