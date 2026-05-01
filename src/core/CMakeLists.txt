add_library(core STATIC
    Logger.cpp
    MathUtils.cpp
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

# ---- SIMD detection --------------------------------------------------------
include(CheckCXXCompilerFlag)
if(MSVC)
    # MSVC exposes all SSE/AVX intrinsics via immintrin.h without explicit flags.
    target_compile_definitions(core PUBLIC CORE_SIMD_ENABLED=1)
else()
    check_cxx_compiler_flag("-msse4.2" COMPILER_HAS_SSE42)
    if(COMPILER_HAS_SSE42)
        target_compile_options(core PUBLIC -msse4.2)
        target_compile_definitions(core PUBLIC CORE_SIMD_ENABLED=1)
        message(STATUS "[core] SSE4.2 enabled")
    else()
        target_compile_definitions(core PUBLIC CORE_SIMD_ENABLED=0)
        message(STATUS "[core] SIMD not available — scalar fallback active")
    endif()
endif()
