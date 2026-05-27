add_library(editor STATIC
    stb_image_write_impl.cpp
    EditorCameraController.cpp
    EditorCommandHistory.cpp
    EditorScene.cpp
    EditorSystem.cpp
    LightWireframeRenderer.cpp
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
    PickingAccelerator.cpp
    PropertiesPanel.cpp
    ResourcesPanel.cpp
    TerrainCreationDialog.cpp
    TerrainEditorPanel.cpp
)

add_subdirectory(commands)

target_include_directories(editor PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/external"
    PRIVATE ${stb_SOURCE_DIR}
)

target_link_libraries(editor PUBLIC game renderer gldevices core imgui imguizmo nfd)
