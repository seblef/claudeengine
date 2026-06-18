add_library(audio STATIC
    SoundSystemFactory.cpp
)

target_include_directories(audio PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(audio PUBLIC abstract)

if(UNIX)
    target_link_libraries(audio PRIVATE openal_audio)
endif()
