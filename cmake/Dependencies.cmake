# libphx third-party dependencies — built from source via FetchContent.
# Pinned versions documented in docs/build-and-maintenance.md

include (FetchContent)
set (FETCHCONTENT_QUIET FALSE)

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
set (PHX_LZ4_INCLUDE_DIR "${lz4_SOURCE_DIR}/lib" CACHE PATH "" FORCE)

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

set (_phx_saved_policy_minimum "${CMAKE_POLICY_VERSION_MINIMUM}")
if (CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
  set (CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif ()
FetchContent_MakeAvailable (bullet)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
  set (CMAKE_POLICY_VERSION_MINIMUM "${_phx_saved_policy_minimum}")
endif ()
set (PHX_BULLET_INCLUDE_DIR "${bullet_SOURCE_DIR}/src" CACHE PATH "" FORCE)

# ---------------------------------------------------------------------------
# LuaJIT — MSVC build via ExternalProject
# ---------------------------------------------------------------------------

# Limit Theory relies on LuaJIT's JIT. LuaJIT does not provide a working JIT
# backend for native Windows ARM64, so reject that target at configure time
# instead of failing deep inside lj_arch.h / buildvm.
if (WIN32)
  set (_phx_target_arch "")
  if (CMAKE_VS_PLATFORM_NAME)
    set (_phx_target_arch "${CMAKE_VS_PLATFORM_NAME}")
  elseif (CMAKE_GENERATOR_PLATFORM)
    set (_phx_target_arch "${CMAKE_GENERATOR_PLATFORM}")
  endif ()
  string (TOUPPER "${_phx_target_arch}" _phx_target_arch_upper)
  if (_phx_target_arch_upper STREQUAL "ARM64"
      OR _phx_target_arch_upper STREQUAL "ARM64EC")
    message (FATAL_ERROR
      "Native Windows ${_phx_target_arch} is not supported: LuaJIT JIT is "
      "required but unavailable on Windows ARM64.\n"
      "On Windows ARM64 hardware, configure for x64 instead:\n"
      "  cmake -S . -B build -A x64\n"
      "The resulting binaries run under Windows x64 emulation with JIT enabled.")
  endif ()
endif ()

include (ExternalProject)

set (LUAJIT_SRC_DIR "${CMAKE_BINARY_DIR}/_deps/luajit-src")
set (LUAJIT_BUILD_DIR "${CMAKE_BINARY_DIR}/_deps/luajit-build")

# Forward the target architecture to the LuaJIT MSVC build. Without this the
# batch script always sets up an x64 toolchain (vcvars64), so lua51.lib is
# built x64 and fails to link against a non-x64 target — e.g.
# "library machine type 'x64' conflicts with target machine type 'ARM64'".
if (CMAKE_VS_PLATFORM_NAME)
  set (LUAJIT_TARGET_ARCH "${CMAKE_VS_PLATFORM_NAME}")
elseif (CMAKE_GENERATOR_PLATFORM)
  set (LUAJIT_TARGET_ARCH "${CMAKE_GENERATOR_PLATFORM}")
else ()
  set (LUAJIT_TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif ()

ExternalProject_Add (luajit_ext
  GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
  GIT_TAG        v2.1.0-beta3
  GIT_SHALLOW    TRUE
  PREFIX         "${CMAKE_BINARY_DIR}/_deps/luajit"
  SOURCE_DIR     "${LUAJIT_SRC_DIR}"
  BINARY_DIR     "${LUAJIT_BUILD_DIR}"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND
    cmd /c "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_luajit_msvc.bat" "${LUAJIT_SRC_DIR}" "${LUAJIT_TARGET_ARCH}"
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

# LuaJIT headers (API); DLL built via ExternalProject below.
set (PHX_LUAJIT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ext/include/luajit" CACHE PATH "" FORCE)

# minimp3 — single-header MP3 decoder (WAV via SDL_LoadWAV)
FetchContent_Declare (minimp3
  GIT_REPOSITORY https://github.com/lieff/minimp3.git
  GIT_TAG        7b590fdcfa5a79c033e76eacc05d0c3e4c79f536
  GIT_SHALLOW    TRUE)

FetchContent_GetProperties (minimp3)
if (NOT minimp3_POPULATED)
  FetchContent_Populate (minimp3)
endif ()
set (PHX_MINIMP3_DIR "${minimp3_SOURCE_DIR}" CACHE PATH "" FORCE)

# ---------------------------------------------------------------------------
# LuaFileSystem — lfs.dll for script I/O helpers (IOEx.lua)
# ---------------------------------------------------------------------------

FetchContent_Declare (lfs
  GIT_REPOSITORY https://github.com/lunarmodules/luafilesystem.git
  GIT_TAG        v1_8_0
  GIT_SHALLOW    TRUE)

FetchContent_GetProperties (lfs)
if (NOT lfs_POPULATED)
  FetchContent_Populate (lfs)
endif ()

add_library (lfs SHARED "${lfs_SOURCE_DIR}/src/lfs.c")
add_dependencies (lfs luajit_ext)
target_include_directories (lfs PRIVATE
  "${PHX_LUAJIT_INCLUDE_DIR}"
  "${LUAJIT_SRC_DIR}/src")
target_link_libraries (lfs lua51)
set_target_properties (lfs PROPERTIES
  PREFIX ""
  RELWITHDEBINFO_POSTFIX ""
  MINSIZEREL_POSTFIX ""
  RELEASE_POSTFIX ""
  OUTPUT_NAME_DEBUG "lfsd"
  OUTPUT_NAME_RELEASE "lfs"
  OUTPUT_NAME_RELWITHDEBINFO "lfs"
  OUTPUT_NAME_MINSIZEREL "lfs")
phx_configure_output_dir (lfs)

# ---------------------------------------------------------------------------
# Helper: copy a shared DLL next to phx/lt at build time
# ---------------------------------------------------------------------------

function (phx_copy_runtime_dll target dll_path)
  add_custom_command (TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${dll_path}" $<TARGET_FILE_DIR:${target}>
    COMMENT "Copying ${dll_path}")
endfunction ()
