add_library(vfx STATIC
    ScreenShake.cpp
    VFXSystem.cpp
)

target_include_directories(vfx PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(vfx PUBLIC core abstract game renderer particles audio physics)
