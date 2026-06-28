cmake_minimum_required (VERSION 3.21)
project (LibPHX)
include (script/build/Shared.cmake)
include (cmake/Dependencies.cmake)

# ------------------------------------------------------------------------------



file (GLOB HEADERS "include/*.h")

file (GLOB SOURCES "src/*.cpp")



add_library (phx SHARED ${SOURCES} ${HEADERS})

add_dependencies (phx luajit_ext)

phx_configure_output_dir (phx)

phx_configure_target_properties (phx)



target_compile_definitions (phx PRIVATE LIBPHX_BUILDING=1 PHX_AUDIO_SDL3=1)



target_include_directories (phx BEFORE PUBLIC "${PHX_BULLET_INCLUDE_DIR}")

target_include_directories (phx BEFORE PUBLIC
  "${freetype_SOURCE_DIR}/include"
  "${freetype_BINARY_DIR}/include")

target_include_directories (phx PUBLIC "include")

# Vendored headers: stb, LuaJIT API, windirent (Windows directory iteration)
target_include_directories (phx PUBLIC "ext/include")

target_include_directories (phx PUBLIC "${PHX_LUAJIT_INCLUDE_DIR}")

target_include_directories (phx PUBLIC "${PHX_LZ4_INCLUDE_DIR}")

target_include_directories (phx PUBLIC "${PHX_MINIMP3_DIR}")



set_target_properties (phx PROPERTIES

  PREFIX "lib"

  RELWITHDEBINFO_POSTFIX ""

  MINSIZEREL_POSTFIX ""

  RELEASE_POSTFIX ""

  OUTPUT_NAME_DEBUG "phx${ARCH}d"

  OUTPUT_NAME_RELEASE "phx${ARCH}"

  OUTPUT_NAME_RELWITHDEBINFO "phx${ARCH}"

  OUTPUT_NAME_MINSIZEREL "phx${ARCH}")



# ------------------------------------------------------------------------------

# Third-party link libraries

# ------------------------------------------------------------------------------



if (WINDOWS)

  target_link_libraries (phx

    opengl32.lib

    user32.lib winmm.lib Ws2_32.lib

    glad

    freetype

    lz4_static

    BulletCollision BulletDynamics LinearMath

    SDL3::SDL3

    lua51)



  phx_copy_runtime_dll (phx "${LUAJIT_SRC_DIR}/src/lua51.dll")

  add_custom_command (TARGET phx POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_if_different

      "$<TARGET_FILE:SDL3::SDL3>" $<TARGET_FILE_DIR:phx>

    COMMENT "Copying SDL3.dll")



elseif (LINUX)

  target_link_libraries (phx

    GL

    dl

    glad

    freetype

    lz4_static

    BulletCollision BulletDynamics LinearMath

    SDL3::SDL3

    lua51)

endif ()



if (MSVC)

  target_link_options (phx PRIVATE "/ignore:4099")

endif ()

