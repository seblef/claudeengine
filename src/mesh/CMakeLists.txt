add_library(mesh STATIC
    EmeshReader.cpp
    EmeshWriter.cpp
    MeshUtils.cpp
)

target_link_libraries(mesh PUBLIC core)
