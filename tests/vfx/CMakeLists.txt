add_executable(vfx_tests
    VehicleFireEffectTest.cpp
    VehicleScrapeEffectTest.cpp
    VFXExplosionTest.cpp
    VFXFireTest.cpp
    VFXScrapeTest.cpp
    VFXSystemTest.cpp
    ${CMAKE_SOURCE_DIR}/src/vfx/VFXSystem.cpp
)

target_include_directories(vfx_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(vfx_tests PRIVATE
    GTest::gtest_main
    core
    game
    renderer
    particles
    audio
    physics
)

target_sources(vfx_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/vfx/ScreenShake.cpp
    ${CMAKE_SOURCE_DIR}/src/vfx/VehicleFireEffect.cpp
    ${CMAKE_SOURCE_DIR}/src/vfx/VehicleScrapeEffect.cpp
    ${CMAKE_SOURCE_DIR}/src/vfx/VFXExplosion.cpp
    ${CMAKE_SOURCE_DIR}/src/vfx/VFXFire.cpp
    ${CMAKE_SOURCE_DIR}/src/vfx/VFXScrape.cpp
)

include(GoogleTest)
gtest_discover_tests(vfx_tests)
