add_library(physics STATIC
    Physics.cpp
    # Additional source files are added as downstream issues are implemented.
)

target_include_directories(physics PUBLIC "${CMAKE_SOURCE_DIR}/src")

# Jolt headers are PRIVATE — they must not leak into consumers (game/, editor/).
target_include_directories(physics PRIVATE "${JoltPhysics_SOURCE_DIR}")

target_link_libraries(physics
    PUBLIC  core
    PRIVATE Jolt
)
