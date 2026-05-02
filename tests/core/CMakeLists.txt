add_executable(core_tests
    Mat2fTest.cpp
    Mat3fTest.cpp
    Mat4fTest.cpp
    Vec2fTest.cpp
    Vec3fTest.cpp
    Vec4fTest.cpp
    ColorTest.cpp
)

target_link_libraries(core_tests PRIVATE
    GTest::gtest_main
    core
)

include(GoogleTest)
gtest_discover_tests(core_tests)
