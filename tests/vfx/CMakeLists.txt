add_executable(vfx_tests
    VFXSystemTest.cpp
    ${CMAKE_SOURCE_DIR}/src/vfx/VFXSystem.cpp
)

target_include_directories(vfx_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(vfx_tests PRIVATE
    GTest::gtest_main
    core
)

include(GoogleTest)
gtest_discover_tests(vfx_tests)
