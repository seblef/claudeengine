add_executable(game_tests
    GameObjectHierarchyTest.cpp
    ${CMAKE_SOURCE_DIR}/src/game/GameObject.cpp
)

target_include_directories(game_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(game_tests PRIVATE
    GTest::gtest_main
    core
)

include(GoogleTest)
gtest_discover_tests(game_tests)
