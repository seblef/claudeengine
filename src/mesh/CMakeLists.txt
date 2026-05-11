add_library(mesh STATIC
    EmeshReader.cpp
    EmeshWriter.cpp
)

target_link_libraries(mesh PUBLIC core)
