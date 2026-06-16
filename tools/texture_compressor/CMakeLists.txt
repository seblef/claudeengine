add_executable(texture_compressor main.cpp)

target_include_directories(texture_compressor PRIVATE
    ${stb_SOURCE_DIR}
    ${gli_SOURCE_DIR}
    ${glm_SOURCE_DIR}
    ${bc7enc_SOURCE_DIR}
)

target_link_libraries(texture_compressor PRIVATE
    bc7enc_lib
    yaml-cpp::yaml-cpp
)

set_target_properties(texture_compressor PROPERTIES
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

if(MSVC)
    target_compile_options(texture_compressor PRIVATE /W3)
else()
    target_compile_options(texture_compressor PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
