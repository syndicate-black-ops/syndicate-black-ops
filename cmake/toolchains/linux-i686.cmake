# CMake toolchain: 32-bit Linux (i686) with the host GCC/Clang in multilib mode (-m32), for cl_dlls/client.so and
# dlls/hl.so.
# The Steam Runtime 3 "sniper" SDK image provides GCC 10 multilib and glibc 2.31.
#
#   cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-i686.cmake -S . -B build/linux
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_C_FLAGS_INIT "-m32")
set(CMAKE_CXX_FLAGS_INIT "-m32")
foreach(_t EXE SHARED MODULE)
  set(CMAKE_${_t}_LINKER_FLAGS_INIT "-m32")
endforeach()
set(CMAKE_LIBRARY_ARCHITECTURE i386-linux-gnu)
