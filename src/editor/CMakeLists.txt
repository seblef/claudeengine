add_library(editor STATIC
    EditorCameraController.cpp
    EditorScene.cpp
    EditorSystem.cpp
    EditorToolbar.cpp
    EditorViewport.cpp
    EditorWindow.cpp
    LogPanel.cpp
    ObjectsPanel.cpp
    ResourcesPanel.cpp
)

target_include_directories(editor PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/external"
)

target_link_libraries(editor PUBLIC game renderer gldevices core imgui imguizmo nfd)
