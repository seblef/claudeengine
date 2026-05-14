add_executable(claude_editor main.cpp)
target_link_libraries(claude_editor PRIVATE editor)
set_target_properties(claude_editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")
