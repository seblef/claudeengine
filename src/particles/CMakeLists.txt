add_library(particles STATIC
    ParticleEmitter.cpp
    ParticleRenderer.cpp
    ParticleSystemTemplate.cpp
)

target_include_directories(particles PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(particles PUBLIC core abstract)
