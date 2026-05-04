add_library(gldevices STATIC
    GLDevices.cpp
    GLShader.cpp
    GLVideoDevice.cpp
    GLVertexBuffer.cpp
)

target_include_directories(gldevices PUBLIC "${CMAKE_SOURCE_DIR}/src")

find_package(OpenGL REQUIRED)
target_link_libraries(gldevices PUBLIC abstract PRIVATE glfw OpenGL::GL)
