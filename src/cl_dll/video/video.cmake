# game_playvideo client video player (src/cl_dll/video, docs/VIDEO_PLAYER.md). Included from cmake/sbo_client.cmake.
#   third-party: libogg + libtheora (decoder) + stb_vorbis, vendored in external/ (external/VIDEO_LIBRARIES.md)
#                -> OBJECT library sbo_video_codecs, their own flags, no warnings, no SBO headers
#   SBO world:   video_player.cpp (HUD/engine), video_decoder.cpp (no engine), video_gl.cpp (OpenGL through
#                pointers, as studio_fullbright_gl.cpp), video_audio.cpp (the engine's SDL2 through pointers)
#                -> client, SBO flags + sbo_compat.h

set(SBO_VIDEO_DIR "${SBO_SRC_DIR}/cl_dll/video")
set(SBO_VIDEO_EXT "${SBO_SRC_DIR}/../external")
get_filename_component(SBO_VIDEO_EXT "${SBO_VIDEO_EXT}" ABSOLUTE)

add_library(sbo_video_codecs OBJECT
  ${SBO_VIDEO_EXT}/libogg/src/bitwise.c
  ${SBO_VIDEO_EXT}/libogg/src/framing.c
  ${SBO_VIDEO_EXT}/libtheora/lib/bitpack.c
  ${SBO_VIDEO_EXT}/libtheora/lib/decinfo.c
  ${SBO_VIDEO_EXT}/libtheora/lib/decode.c
  ${SBO_VIDEO_EXT}/libtheora/lib/dequant.c
  ${SBO_VIDEO_EXT}/libtheora/lib/fragment.c
  ${SBO_VIDEO_EXT}/libtheora/lib/huffdec.c
  ${SBO_VIDEO_EXT}/libtheora/lib/idct.c
  ${SBO_VIDEO_EXT}/libtheora/lib/info.c
  ${SBO_VIDEO_EXT}/libtheora/lib/internal.c
  ${SBO_VIDEO_EXT}/libtheora/lib/quant.c
  ${SBO_VIDEO_EXT}/libtheora/lib/state.c
  ${SBO_VIDEO_DIR}/stb_vorbis_impl.c)
target_include_directories(sbo_video_codecs PRIVATE
  ${SBO_VIDEO_EXT}/libogg/include ${SBO_VIDEO_EXT}/libtheora/include ${SBO_VIDEO_EXT}/stb_vorbis)
set_target_properties(sbo_video_codecs PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(WIN32)
  target_compile_definitions(sbo_video_codecs PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(sbo_video_codecs PRIVATE /Zi /W0)
else()
  target_compile_options(sbo_video_codecs PRIVATE -fPIC -fvisibility=hidden -w)
endif()
target_sources(client PRIVATE $<TARGET_OBJECTS:sbo_video_codecs>)

set(_video_sbo_sources ${SBO_VIDEO_DIR}/video_player.cpp ${SBO_VIDEO_DIR}/video_decoder.cpp
  ${SBO_VIDEO_DIR}/video_gl.cpp ${SBO_VIDEO_DIR}/video_audio.cpp)
target_sources(client PRIVATE ${_video_sbo_sources})
set_source_files_properties(${_video_sbo_sources} PROPERTIES COMPILE_OPTIONS "${SBO_CLIENT_COMPAT_FI}")
target_include_directories(client PRIVATE ${SBO_VIDEO_DIR}
  ${SBO_VIDEO_EXT}/libogg/include ${SBO_VIDEO_EXT}/libtheora/include ${SBO_VIDEO_EXT}/stb_vorbis)

# video_gl.cpp: opengl32 on Windows is linked by worldscreen.cmake (GetModuleHandle needs only the loaded module);
# Linux: SDL_GL_GetProcAddress and dlsym (SDL2 and libdl are linked already)
