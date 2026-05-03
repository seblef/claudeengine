add_executable(claude_engine
    main.cpp
)

target_link_libraries(claude_engine PRIVATE core gldevices)
