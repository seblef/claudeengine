add_executable(track_tests
    TerrainTileTest.cpp
    ${CMAKE_SOURCE_DIR}/src/track/TerrainTile.cpp
)

target_include_directories(track_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(track_tests PRIVATE
    GTest::gtest_main
    core
)

include(GoogleTest)
gtest_discover_tests(track_tests)
