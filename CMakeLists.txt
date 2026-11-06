# SBO (Syndicate Black Ops) build.
#
# Targets:
#   client  client.dll   Windows (MSVC ABI: clang-cl + lld-link + xwin CRT/SDK in docker, or cl.exe VS 2019+)
#           client.so    Linux, 32-bit (gcc -m32; docs/dev/LINUX_CLIENT.md)
#   server  hl.dll       Windows (same toolchain as the client)
#           hl.so        Linux, 32-bit (gcc/clang -m32)
#
# Build wrapper: scripts/build.sh (runs the builds below in their docker images).
# Toolchains: docker/clang-cl (Windows targets), Steam Runtime 3 "sniper" SDK (Linux targets, glibc 2.31).
cmake_minimum_required(VERSION 3.25)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Debug, Release, RelWithDebInfo, MinSizeRel" FORCE)
endif()

# Static CRT (/MT, /MTd in Debug) like SBO's VC6 build and BugfixedHL-Rebased. Everything linked into client.dll
# (incl. hl1_source_sdk static libs such as vgui_controls/tier1) must use the same CRT.
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Game version: shown in the client (Version tab) as <version>+<commit> and by the server's game_version cvar as
# "<version>+<commit> <branch>", see cmake/sbo_version.cmake
project(sbo VERSION 2.0.0 LANGUAGES C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "SBO's binaries must be 32-bit (x86). Got pointer size ${CMAKE_SIZEOF_VOID_P}.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SBO_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(WIN32)
  if(NOT MSVC)
    message(FATAL_ERROR "SBO's Windows binaries need an MSVC-ABI compiler (clang-cl or cl.exe); use "
                        "cmake/toolchains/clang-cl-i686.cmake or a Visual Studio Win32 generator.")
  endif()
endif()
option(SBO_BUILD_CLIENT "Build the client (client.dll on Windows, client.so on Linux)" ON)
option(SBO_BUILD_SERVER "Build the server (hl.dll on Windows, hl.so on Linux)" ON)
# The test layer (test_* commands, trace; docs/dev/ENTITY_TESTS.md): OFF by default, so a plain build has none (owner
# 2026-10-17); builds for the entity tests, scripted client tests and test_* on a server pass -DSBO_TEST_COMMANDS=ON.
# Release CI passes OFF explicitly; scripts/check_no_test_commands.sh checks the release binaries. A build directory
# configured before this change keeps ON in its CMakeCache.txt until reconfigured with -DSBO_TEST_COMMANDS=OFF.
option(SBO_TEST_COMMANDS "Compile in the test_* commands for the automated entity tests" OFF)

# Build version header (sbo_version_gen.h: version, commit, branch, CI flag, build date), regenerated on every build.
# The version is project(VERSION) above; a release tag v<version> must match it (scripts/updater_release.py checks).
# SBO_VERSION_OVERRIDE: updater test builds only (docs/UPDATER.md), e.g. 2.0.1-test1; empty for every other build.
set(SBO_VERSION_OVERRIDE "" CACHE STRING "Test builds only: this version instead of project(VERSION)")
if(SBO_VERSION_OVERRIDE)
  set(SBO_VERSION_EFFECTIVE "${SBO_VERSION_OVERRIDE}")
else()
  set(SBO_VERSION_EFFECTIVE "${PROJECT_VERSION}")
endif()
set(SBO_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
add_custom_target(sbo_version_gen
  COMMAND ${CMAKE_COMMAND} -DOUT=${SBO_GENERATED_DIR}/sbo_version_gen.h -DVERSION=${SBO_VERSION_EFFECTIVE}
          -DSRC=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/sbo_version.cmake
  BYPRODUCTS ${SBO_GENERATED_DIR}/sbo_version_gen.h
  VERBATIM)

if(SBO_BUILD_CLIENT)
  include(cmake/sbo_client.cmake)
endif()
if(SBO_BUILD_SERVER)
  include(cmake/sbo_server.cmake)
endif()
