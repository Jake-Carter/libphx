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
    target_compile_options (${target} PRIVATE "/arch:SSE2")
    # Suppress VS default /EHsc conflicting with /EHs-c-
    string (REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string (REPLACE "/EHsc" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string (REPLACE "/EHsc" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string (REPLACE "/EHsc" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    string (REPLACE "/EHsc" "" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
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
