add_library(core STATIC
    Logger.cpp
)

if(BUILD_PROFILE STREQUAL "stable")
    target_compile_definitions(core PUBLIC
        LOGURU_DEBUG_LOGGING=0
        LOGURU_VERBOSE_SCOPE_ENDINGS=0
    )
else()
    target_compile_definitions(core PUBLIC
        LOGURU_DEBUG_LOGGING=1
        LOGURU_VERBOSE_SCOPE_ENDINGS=1
    )
endif()

# Include root: enables #include "core/Logger.h" in all consumers (Google Style).
target_include_directories(core PUBLIC "${CMAKE_SOURCE_DIR}/src")

# loguru is PUBLIC so modules linking core get logging transitively.
target_link_libraries(core PUBLIC loguru)
