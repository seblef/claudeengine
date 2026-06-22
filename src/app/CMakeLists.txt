add_executable(wreckoning
    main.cpp
)

target_link_libraries(wreckoning PRIVATE core gldevices renderer game audio openal)
