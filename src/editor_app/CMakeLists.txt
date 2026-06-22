add_executable(wreckoning_editor main.cpp)
target_link_libraries(wreckoning_editor PRIVATE editor)
set_target_properties(wreckoning_editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")
