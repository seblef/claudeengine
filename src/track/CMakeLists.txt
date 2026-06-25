add_library(track STATIC
    RoadSpline.cpp
    RoadMeshBuilder.cpp
    TireTrackSystem.cpp
)

target_include_directories(track PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(track PUBLIC core abstract physics)
