if (WIN32)
  set (PLATFORM "win")
  set (WINDOWS TRUE)
elseif (UNIX AND NOT APPLE)
  set (PLATFORM "linux")
  set (LINUX TRUE)
else ()
  message (FATAL_ERROR "Unsupported Platform")
endif ()

if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "4")
  set (ARCH "32")
elseif ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set (ARCH "64")
else ()
  message (FATAL_ERROR "Unsupported CPU Architecture")
endif ()

set (PLATARCH "${PLATFORM}${ARCH}")

# Windows target platform from the generator (-A ARM64EC, -A x64, etc.).
set (PHX_WIN_TARGET_ARCH "")
set (PHX_WIN_TARGET_ARCH_UPPER "")
set (PHX_WINDOWS_ARM FALSE)
set (PHX_ARM64EC FALSE)
if (WINDOWS)
  if (CMAKE_VS_PLATFORM_NAME)
    set (PHX_WIN_TARGET_ARCH "${CMAKE_VS_PLATFORM_NAME}")
  elseif (CMAKE_GENERATOR_PLATFORM)
    set (PHX_WIN_TARGET_ARCH "${CMAKE_GENERATOR_PLATFORM}")
  endif ()
  string (TOUPPER "${PHX_WIN_TARGET_ARCH}" PHX_WIN_TARGET_ARCH_UPPER)
  if (PHX_WIN_TARGET_ARCH_UPPER STREQUAL "ARM64"
      OR PHX_WIN_TARGET_ARCH_UPPER STREQUAL "ARM64EC")
    set (PHX_WINDOWS_ARM TRUE)
  endif ()
  if (PHX_WIN_TARGET_ARCH_UPPER STREQUAL "ARM64EC")
    set (PHX_ARM64EC TRUE)
  endif ()
endif ()

# MSVC defaults to /EHsc; project uses /EHs-c- (no exceptions). Strip /EHsc from
# global flags so D9025 override warnings do not spam every translation unit.
if (WINDOWS)
  foreach (_phx_flag_var
      CMAKE_CXX_FLAGS
      CMAKE_CXX_FLAGS_DEBUG
      CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_RELWITHDEBINFO
      CMAKE_CXX_FLAGS_MINSIZEREL)
    if (DEFINED ${_phx_flag_var})
      string (REPLACE "/EHsc" "" ${_phx_flag_var} "${${_phx_flag_var}}")
    endif ()
  endforeach ()
endif ()

function (phx_configure_output_dir target)
  set_target_properties (${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

  foreach (config ${CMAKE_CONFIGURATION_TYPES})
    string (TOUPPER ${config} config)
    set_target_properties (${target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_SOURCE_DIR}/bin"
      LIBRARY_OUTPUT_DIRECTORY_${config} "${CMAKE_SOURCE_DIR}/bin"
      ARCHIVE_OUTPUT_DIRECTORY_${config} "${CMAKE_SOURCE_DIR}/bin")
  endforeach (config)
endfunction ()

function (phx_configure_target_properties target)
  if (WINDOWS)
    target_compile_definitions (${target} PRIVATE _CRT_SECURE_NO_DEPRECATE)
    target_compile_definitions (${target} PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions (${target} PRIVATE WINDOWS=1)

    target_compile_options (${target} PRIVATE "/MP")
    target_compile_options (${target} PRIVATE "/MD")
    target_compile_options (${target} PRIVATE "/EHs-c-")
    target_compile_options (${target} PRIVATE "/fp:fast")
    target_compile_options (${target} PRIVATE "$<$<CONFIG:Release>:/GL>")
    target_compile_options (${target} PRIVATE "/GS-")
    target_compile_options (${target} PRIVATE "/GR-")
    if (NOT PHX_WINDOWS_ARM)
      target_compile_options (${target} PRIVATE "/arch:SSE2")
    endif ()
    if (PHX_ARM64EC)
      # Match the Bullet targets on ARM64EC (see cmake/Dependencies.cmake):
      # disable Bullet's x64 SSE path so btVector3 / btMatrix3x3 layouts stay
      # ABI-consistent, and route the residual <emmintrin.h> include in
      # btScalar.h through <intrin.h> so it compiles.
      target_compile_definitions (${target} PRIVATE __BT_DISABLE_SSE__)
      target_compile_options (${target} PRIVATE "/FIintrin.h")
    endif ()
    # Residual D9025 if a TU still sees /EHsc from toolset defaults
    target_compile_options (${target} PRIVATE "/wd9025")
  elseif (LINUX)
    target_compile_definitions (${target} PRIVATE UNIX=1)

    target_compile_options (${target} PRIVATE "-Wall")            # All error checking
    target_compile_options (${target} PRIVATE "-fno-exceptions")  # No exception handling
    target_compile_options (${target} PRIVATE "-ffast-math")      # No strict FP
    target_compile_options (${target} PRIVATE "-fpic")            # PIC since this is shared

    target_compile_options (${target} PRIVATE "-Wno-unused-variable")
    target_compile_options (${target} PRIVATE "-Wno-unknown-pragmas")

    # Aggressive optimization, assuming SSE4+
    target_compile_options (${target} PRIVATE "-O3")
    target_compile_options (${target} PRIVATE "-msse")
    target_compile_options (${target} PRIVATE "-msse2")
    target_compile_options (${target} PRIVATE "-msse3")
    target_compile_options (${target} PRIVATE "-msse4")

    # :(
    target_compile_options (${target} PRIVATE "-std=c++11")

  endif ()
endfunction ()

# Sets DEBUG=1 for Debug builds, DEBUG=0 otherwise. The lt executable passes
# this to Lua as __debug__, which selects libphx64d vs libphx64 at load time.
function (phx_configure_debug target)
  target_compile_definitions (${target} PRIVATE
    $<$<CONFIG:Debug>:DEBUG=1>
    $<$<NOT:$<CONFIG:Debug>>:DEBUG=0>)
endfunction ()
