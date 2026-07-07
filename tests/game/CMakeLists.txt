add_executable(game_tests
    GameObjectHierarchyTest.cpp
    VehicleCrashSoundTest.cpp
    VehicleDamageTest.cpp
    ${CMAKE_SOURCE_DIR}/src/game/GameObject.cpp
    ${CMAKE_SOURCE_DIR}/src/game/SoundEffectComponent.cpp
    ${CMAKE_SOURCE_DIR}/src/game/VehicleCrashSound.cpp
    ${CMAKE_SOURCE_DIR}/src/game/VehicleDamage.cpp
)

target_include_directories(game_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(game_tests PRIVATE
    GTest::gtest_main
    core
    audio
)

include(GoogleTest)
gtest_discover_tests(game_tests)
