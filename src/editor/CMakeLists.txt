add_library(editor STATIC
    EditorSystem.cpp
)

target_include_directories(editor PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(editor PUBLIC game renderer gldevices core imgui imguizmo nfd)
