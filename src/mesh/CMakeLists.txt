add_library(mesh STATIC
    EmeshReader.cpp
    EmeshWriter.cpp
    MeshUtils.cpp
    ObjImporter.cpp
)

target_link_libraries(mesh PUBLIC core fast_obj)
