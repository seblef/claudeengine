add_library(physics STATIC
    Physics.cpp
    PhysicsBody.cpp
    PhysicsSystem.cpp
)

target_include_directories(physics PUBLIC "${CMAKE_SOURCE_DIR}/src")

# Jolt's own CMakeLists exports its source root as a PUBLIC include, so
# linking PRIVATE against the Jolt target is enough to compile Jolt headers.

target_link_libraries(physics
    PUBLIC  core
    PRIVATE Jolt terrain
)
