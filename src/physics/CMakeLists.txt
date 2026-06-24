add_library(physics STATIC
    CharacterController.cpp
    JoltDebugRenderer.cpp
    Physics.cpp
    PhysicsBody.cpp
    PhysicsShapeCache.cpp
    PhysicsSystem.cpp
    PhysicsVehicle.cpp
)

# JoltDebugRenderer subclasses JPH::DebugRenderer which was compiled with
# -fno-rtti; the inheriting TU must match to avoid a missing-typeinfo link error.
set_source_files_properties(JoltDebugRenderer.cpp PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)

target_include_directories(physics PUBLIC "${CMAKE_SOURCE_DIR}/src")

# Jolt's own CMakeLists exports its source root as a PUBLIC include, so
# linking PRIVATE against the Jolt target is enough to compile Jolt headers.

target_link_libraries(physics
    PUBLIC  core
    PRIVATE Jolt terrain renderer
)
