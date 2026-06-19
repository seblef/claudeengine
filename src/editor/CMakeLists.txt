add_library(editor STATIC
    stb_image_impl.cpp
    stb_image_write_impl.cpp
    tinyexr_impl.cpp
    ${tinyexr_SOURCE_DIR}/deps/miniz/miniz.c
    EditorCameraController.cpp
    EditorCommandHistory.cpp
    EditorScene.cpp
    EditorSystem.cpp
    EnvironmentEditorPanel.cpp
    PlayerStartGizmos.cpp
    MapPropertiesWindow.cpp
    MapSerializer.cpp
    EditorTheme.cpp
    EditorToolbar.cpp
    EditorViewport.cpp
    EditorWindow.cpp
    LogPanel.cpp
    MaterialEditorWindow.cpp
    MeshEditorWindow.cpp
    ParticleEditorWindow.cpp
    SoundEditorWindow.cpp
    MeshPreview.cpp
    MeshSelectionModal.cpp
    ParticleSystemSelectionModal.cpp
    SoundEmitterSelectionModal.cpp
    ObjectNamingUtils.cpp
    ObjectsPanel.cpp
    PickingAccelerator.cpp
    PropertiesPanel.cpp
    RenderingSettingsPanel.cpp
    ResourcesPanel.cpp
    TerrainCreationDialog.cpp
    TerrainEditorPanel.cpp
    TerrainPainterWindow.cpp
)

add_subdirectory(commands)
add_subdirectory(tools)

target_include_directories(editor PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/external"
    PRIVATE ${stb_SOURCE_DIR}
    PRIVATE ${tinyexr_SOURCE_DIR}
    PRIVATE ${tinyexr_SOURCE_DIR}/deps/miniz
)

target_link_libraries(editor PUBLIC game renderer gldevices core mesh imgui imguizmo nfd)
