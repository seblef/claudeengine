add_library(audio STATIC
    SoundSystemFactory.cpp
    Sound.cpp
    AudioDecoderUtils.cpp
    ResourceManager.cpp
    VirtualSoundInstance.cpp
    SoundManager.cpp
)

target_include_directories(audio PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(audio PUBLIC abstract yaml-cpp)
target_link_libraries(audio PRIVATE dr_libs loguru)

# stb_vorbis.c is included directly in AudioDecoderUtils.cpp.
target_include_directories(audio PRIVATE "${stb_SOURCE_DIR}")

if(UNIX)
    target_link_libraries(audio PRIVATE openal_audio)
endif()

if(OPUSFILE_FOUND)
    target_compile_definitions(audio PRIVATE HAVE_OPUSFILE)
    target_include_directories(audio PRIVATE ${OPUSFILE_INCLUDE_DIRS})
    target_link_libraries(audio PRIVATE ${OPUSFILE_LIBRARIES})
endif()
