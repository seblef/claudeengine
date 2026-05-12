add_executable(mesh_tests
    ObjImporterTest.cpp
)

target_link_libraries(mesh_tests PRIVATE
    GTest::gtest_main
    mesh
)

target_compile_definitions(mesh_tests PRIVATE
    MESH_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

include(GoogleTest)
gtest_discover_tests(mesh_tests)
