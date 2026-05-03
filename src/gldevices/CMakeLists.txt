add_library(gldevices STATIC
    GLDevices.cpp
)

target_include_directories(gldevices PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(gldevices PUBLIC abstract PRIVATE glfw)
