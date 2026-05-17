add_library(editor STATIC
    EditorCameraController.cpp
    EditorCommandHistory.cpp
    EditorScene.cpp
    EditorSystem.cpp
    MapPropertiesWindow.cpp
    MapSerializer.cpp
    EditorTheme.cpp
    EditorToolbar.cpp
    EditorViewport.cpp
    EditorWindow.cpp
    LogPanel.cpp
    MaterialEditorWindow.cpp
    MeshPreview.cpp
    MeshSelectionModal.cpp
    ObjectsPanel.cpp
    PropertiesPanel.cpp
    ResourcesPanel.cpp
)

add_subdirectory(commands)

target_include_directories(editor PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/external"
)

target_link_libraries(editor PUBLIC game renderer gldevices core imgui imguizmo nfd)
