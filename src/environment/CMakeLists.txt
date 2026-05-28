add_library(environment STATIC
    SkyRenderer.cpp
    WaterRenderer.cpp
    WindSystem.cpp
    WorldTime.cpp
)

target_include_directories(environment PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(environment PUBLIC core abstract)
