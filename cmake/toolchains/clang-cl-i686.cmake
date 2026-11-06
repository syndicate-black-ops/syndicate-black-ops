# CMake toolchain: clang-cl + lld-link cross-compiling to 32-bit Windows (i686-pc-windows-msvc, MSVC ABI) on Linux,
# against the MSVC CRT + Windows SDK repacked by xwin (docker/clang-cl/Dockerfile puts them in /opt/xwin).
#
#   cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/clang-cl-i686.cmake -S . -B build/modern/cmake
#
# XWIN_ROOT (env or cache) overrides /opt/xwin. Expected layout (xwin splat):
#   crt/include  crt/lib/x86  sdk/include/{ucrt,um,shared,winrt}  sdk/lib/{ucrt,um}/x86
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR X86)

if(NOT XWIN_ROOT)
  if(DEFINED ENV{XWIN_ROOT})
    set(XWIN_ROOT "$ENV{XWIN_ROOT}")
  else()
    set(XWIN_ROOT /opt/xwin)
  endif()
endif()
set(XWIN_ROOT "${XWIN_ROOT}" CACHE PATH "xwin splat output (MSVC CRT + Windows SDK)")
# try_compile() projects re-read this file; pass XWIN_ROOT on to them
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES XWIN_ROOT)

set(SBO_CLANG_TARGET i686-pc-windows-msvc)
set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_C_COMPILER_TARGET ${SBO_CLANG_TARGET})
set(CMAKE_CXX_COMPILER_TARGET ${SBO_CLANG_TARGET})
set(CMAKE_LINKER lld-link)
set(CMAKE_AR llvm-lib)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT llvm-mt)

# /imsvc = system include dirs (warnings suppressed), searched like %INCLUDE%
set(_sbo_sysinc
  "/imsvc${XWIN_ROOT}/crt/include"
  "/imsvc${XWIN_ROOT}/sdk/include/ucrt"
  "/imsvc${XWIN_ROOT}/sdk/include/um"
  "/imsvc${XWIN_ROOT}/sdk/include/shared"
  "/imsvc${XWIN_ROOT}/sdk/include/winrt")
list(JOIN _sbo_sysinc " " _sbo_sysinc)
set(CMAKE_C_FLAGS_INIT "${_sbo_sysinc}")
set(CMAKE_CXX_FLAGS_INIT "${_sbo_sysinc}")
set(CMAKE_RC_FLAGS_INIT "-I${XWIN_ROOT}/sdk/include/um -I${XWIN_ROOT}/sdk/include/shared")

set(_sbo_libpath
  "/libpath:${XWIN_ROOT}/crt/lib/x86"
  "/libpath:${XWIN_ROOT}/sdk/lib/um/x86"
  "/libpath:${XWIN_ROOT}/sdk/lib/ucrt/x86")
list(JOIN _sbo_libpath " " _sbo_libpath)
foreach(_t EXE SHARED MODULE)
  set(CMAKE_${_t}_LINKER_FLAGS_INIT "${_sbo_libpath}")
endforeach()

set(CMAKE_FIND_ROOT_PATH "${XWIN_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
