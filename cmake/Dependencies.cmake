# libphx third-party dependencies — built from source via FetchContent.
# Pinned versions documented in docs/build-and-maintenance.md

include (FetchContent)
set (FETCHCONTENT_QUIET FALSE)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option (PHX_ENABLE_FMOD "Link FMOD audio (legacy); default is SDL3 audio backend" OFF)

# ---------------------------------------------------------------------------
# SDL3
# ---------------------------------------------------------------------------

set (SDL_SHARED ON CACHE BOOL "" FORCE)
set (SDL_STATIC OFF CACHE BOOL "" FORCE)
set (SDL_TEST OFF CACHE BOOL "" FORCE)
set (SDL_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare (SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-3.2.8
  GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable (SDL3)

# ---------------------------------------------------------------------------
# GLAD — OpenGL 2.1 compatibility profile (vendored generated loader)
# ---------------------------------------------------------------------------

add_library (glad STATIC "${CMAKE_CURRENT_SOURCE_DIR}/ext/glad/src/gl.c")
target_include_directories (glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/ext/glad/include")

# ---------------------------------------------------------------------------
# FreeType (static)
# ---------------------------------------------------------------------------

set (FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set (FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set (FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set (FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set (FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set (BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare (freetype
  GIT_REPOSITORY https://github.com/freetype/freetype.git
  GIT_TAG        VER-2-13-3
  GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable (freetype)

# ---------------------------------------------------------------------------
# LZ4 (static)
# ---------------------------------------------------------------------------

set (LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set (LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)

FetchContent_Declare (lz4
  GIT_REPOSITORY https://github.com/lz4/lz4.git
  GIT_TAG        v1.10.0
  GIT_SHALLOW    TRUE
  SOURCE_SUBDIR  build/cmake)

FetchContent_MakeAvailable (lz4)

# ---------------------------------------------------------------------------
# Bullet Physics (static)
# ---------------------------------------------------------------------------

set (BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
set (BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set (BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set (BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
set (BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set (BUILD_BULLET3 OFF CACHE BOOL "" FORCE)
set (USE_GRAPHICAL_BENCHMARK OFF CACHE BOOL "" FORCE)
set (USE_MSVC_RUNTIME_LIBRARY_DLL ON CACHE BOOL "" FORCE)
set (BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare (bullet
  GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
  GIT_TAG        3.25
  GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable (bullet)

# ---------------------------------------------------------------------------
# LuaJIT — MSVC build via ExternalProject
# ---------------------------------------------------------------------------

include (ExternalProject)

set (LUAJIT_SRC_DIR "${CMAKE_BINARY_DIR}/_deps/luajit-src")
set (LUAJIT_BUILD_DIR "${CMAKE_BINARY_DIR}/_deps/luajit-build")

ExternalProject_Add (luajit_ext
  GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
  GIT_TAG        v2.1-20250116.0
  GIT_SHALLOW    TRUE
  PREFIX         "${CMAKE_BINARY_DIR}/_deps/luajit"
  SOURCE_DIR     "${LUAJIT_SRC_DIR}"
  BINARY_DIR     "${LUAJIT_BUILD_DIR}"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND
    ${CMAKE_COMMAND} -E env "PATH=$ENV{PATH}"
    cmd /c "cd /d ${LUAJIT_SRC_DIR}/src && msvcbuild.bat"
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS
    "${LUAJIT_SRC_DIR}/src/lua51.lib"
    "${LUAJIT_SRC_DIR}/src/lua51.dll"
  UPDATE_COMMAND "")

add_library (lua51 SHARED IMPORTED GLOBAL)
add_dependencies (lua51 luajit_ext)
set_target_properties (lua51 PROPERTIES
  IMPORTED_IMPLIB   "${LUAJIT_SRC_DIR}/src/lua51.lib"
  IMPORTED_LOCATION "${LUAJIT_SRC_DIR}/src/lua51.dll")

# LuaJIT headers ship with the source tree
set (PHX_LUAJIT_INCLUDE_DIR "${LUAJIT_SRC_DIR}/src" CACHE PATH "" FORCE)

# ---------------------------------------------------------------------------
# dr_wav / dr_mp3 — single-header decoders (header-only)
# ---------------------------------------------------------------------------

FetchContent_Declare (dr_libs
  GIT_REPOSITORY https://github.com/mackron/dr_libs.git
  GIT_TAG        master
  GIT_SHALLOW    TRUE)

FetchContent_GetProperties (dr_libs)
if (NOT dr_libs_POPULATED)
  FetchContent_Populate (dr_libs)
  set (PHX_DR_LIBS_DIR "${dr_libs_SOURCE_DIR}" CACHE PATH "" FORCE)
endif ()

# ---------------------------------------------------------------------------
# Helper: copy a shared DLL next to phx/lt at build time
# ---------------------------------------------------------------------------

function (phx_copy_runtime_dll target dll_path)
  add_custom_command (TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${dll_path}" $<TARGET_FILE_DIR:${target}>
    COMMENT "Copying ${dll_path}")
endfunction ()
