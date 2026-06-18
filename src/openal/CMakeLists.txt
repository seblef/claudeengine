find_package(OpenAL REQUIRED)

add_library(openal_audio STATIC
    ALSoundBuffer.cpp
    ALSoundSource.cpp
    ALSoundSystem.cpp
)

target_include_directories(openal_audio PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(openal_audio PUBLIC abstract PRIVATE OpenAL::OpenAL)
