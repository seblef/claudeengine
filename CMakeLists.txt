cmake_minimum_required(VERSION 3.21)
project(ClaudeEngine LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Build profile: "dev" (default) or "stable"
# ---------------------------------------------------------------------------
set(BUILD_PROFILE "dev" CACHE STRING "Build profile: dev or stable")
set_property(CACHE BUILD_PROFILE PROPERTY STRINGS "dev" "stable")

if(BUILD_PROFILE STREQUAL "stable")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    message(STATUS "[ClaudeEngine] Profile: stable (Release, O3, LTO)")
else()
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
    message(STATUS "[ClaudeEngine] Profile: dev (Debug, O0, verbose logging)")
endif()

# ---------------------------------------------------------------------------
# Global C++ standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Binary always lands in build/ at the repository root.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

# ---------------------------------------------------------------------------
# LTO for stable profile
# ---------------------------------------------------------------------------
if(BUILD_PROFILE STREQUAL "stable")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "[ClaudeEngine] LTO enabled")
    else()
        message(WARNING "[ClaudeEngine] LTO not supported: ${ipo_error}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Fetch loguru into external/loguru-src/
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/external")
# CMake 4.x dropped support for cmake_minimum_required < 3.5; enforce 3.5 as
# the floor for all fetched sub-projects (e.g. yaml-cpp 0.8.0 declares 3.4).
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

FetchContent_Declare(
    loguru
    GIT_REPOSITORY https://github.com/emilk/loguru.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
set(LOGURU_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LOGURU_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(LOGURU_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(loguru)

# ---------------------------------------------------------------------------
# Fetch GLFW into external/glfw-src/
# ---------------------------------------------------------------------------
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND  OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11      ON  CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# ---------------------------------------------------------------------------
# Fetch yaml-cpp into external/yaml-cpp-src/
# ---------------------------------------------------------------------------
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
    GIT_SHALLOW    TRUE
)
set(YAML_CPP_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS   OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL       OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# ---------------------------------------------------------------------------
# fast_obj (single-header OBJ loader)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    fast_obj
    GIT_REPOSITORY https://github.com/thisistherk/fast_obj.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(fast_obj)

# ---------------------------------------------------------------------------
# ufbx (single-file FBX loader)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    ufbx
    GIT_REPOSITORY https://github.com/ufbx/ufbx.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ufbx)

# ufbx has no CMakeLists.txt — create a static library target manually.
add_library(ufbx STATIC ${ufbx_SOURCE_DIR}/ufbx.c)
target_include_directories(ufbx PUBLIC ${ufbx_SOURCE_DIR})
if(MSVC)
    target_compile_options(ufbx PRIVATE /W0)
else()
    target_compile_options(ufbx PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
# stb (single-header image loader) — JPEG and PNG support
# ---------------------------------------------------------------------------
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

# ---------------------------------------------------------------------------
# GLM (header-only math library, required by GLI)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(glm)

# ---------------------------------------------------------------------------
# GLI (header-only DDS/KTX compressed-texture loader)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    gli
    GIT_REPOSITORY https://github.com/g-truc/gli.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(gli)

# ---------------------------------------------------------------------------
# bc7enc (BC7 block compressor used by texture_compressor tool)
# Uses FetchContent_Populate (not MakeAvailable) to download the sources
# without invoking bc7enc's own CMakeLists.txt, which creates an executable
# that would conflict with our own bc7enc_lib target.
# ---------------------------------------------------------------------------
FetchContent_Declare(
    bc7enc
    GIT_REPOSITORY https://github.com/richgel999/bc7enc.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(bc7enc)
if(NOT bc7enc_POPULATED)
    FetchContent_Populate(bc7enc)
endif()

add_library(bc7enc_lib STATIC ${bc7enc_SOURCE_DIR}/bc7enc.c)
target_include_directories(bc7enc_lib PUBLIC ${bc7enc_SOURCE_DIR})
if(MSVC)
    target_compile_options(bc7enc_lib PRIVATE /W0)
else()
    target_compile_options(bc7enc_lib PRIVATE -w)
endif()

# Optional TIFF support via system libtiff
find_package(TIFF QUIET)
if(TIFF_FOUND AND TARGET TIFF::TIFF)
    # Ubuntu 26.04's libtiff CMake config omits IMPORTED_LOCATION for the Debug
    # configuration; fall back to the Release location so Debug builds link.
    get_target_property(_tiff_dbg TIFF::TIFF IMPORTED_LOCATION_DEBUG)
    if(NOT _tiff_dbg)
        get_target_property(_tiff_rel TIFF::TIFF IMPORTED_LOCATION_RELEASE)
        if(NOT _tiff_rel)
            set(_tiff_rel "${TIFF_LIBRARIES}")
        endif()
        set_target_properties(TIFF::TIFF PROPERTIES IMPORTED_LOCATION_DEBUG "${_tiff_rel}")
    endif()
    message(STATUS "[ClaudeEngine] libtiff found — TIFF texture support enabled")
    add_compile_definitions(HAVE_LIBTIFF)
endif()

# ---------------------------------------------------------------------------
# Editor-only dependencies: ImGui, ImGuizmo, nfd
# Guarded by BUILD_EDITOR so claude_engine builds without these on machines
# that lack the editor system requirements (e.g. libgtk-3-dev on Linux).
# ---------------------------------------------------------------------------
option(BUILD_EDITOR "Build the editor executable (requires libgtk-3-dev on Linux)" OFF)

if(BUILD_EDITOR)
    find_package(OpenGL REQUIRED)

    # ---- ImGui (docking branch) ---------------------------------------------
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        docking
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(imgui)
    message(STATUS "[ClaudeEngine] ImGui (docking) fetched")

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
    if(MSVC)
        target_compile_options(imgui PRIVATE /W0)
    else()
        target_compile_options(imgui PRIVATE -w)
    endif()

    # ---- ImGuizmo -----------------------------------------------------------
    # ImGuizmo's own CMakeLists.txt creates the imguizmo target; we only need
    # to link it against our imgui so it can find imgui.h at build time.
    FetchContent_Declare(
        imguizmo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(imguizmo)
    message(STATUS "[ClaudeEngine] ImGuizmo fetched")
    target_link_libraries(imguizmo PUBLIC imgui)
    if(MSVC)
        target_compile_options(imguizmo PRIVATE /W0)
    else()
        target_compile_options(imguizmo PRIVATE -w)
    endif()

    # ---- nfd (native file dialog extended) ----------------------------------
    FetchContent_Declare(
        nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    set(NFD_BUILD_TESTS  OFF CACHE BOOL "" FORCE)
    set(NFD_WAYLAND      OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(nfd)
    message(STATUS "[ClaudeEngine] nfd (native file dialog) fetched")

    # ---- TinyEXR (single-header OpenEXR loader) -----------------------------
    # EXCLUDE_FROM_ALL: we only need the headers; skip TinyEXR's own targets.
    FetchContent_Declare(
        tinyexr
        GIT_REPOSITORY https://github.com/syoyo/tinyexr.git
        GIT_TAG        v1.0.13
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(tinyexr)
    message(STATUS "[ClaudeEngine] TinyEXR fetched")
endif()

add_subdirectory(src)
add_subdirectory(tools)

# ---------------------------------------------------------------------------
# Google Test via FetchContent
# ---------------------------------------------------------------------------
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK   OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_subdirectory(tests)
