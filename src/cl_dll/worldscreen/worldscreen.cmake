# World-space VGUI2 screens (src/cl_dll/worldscreen/worldscreen.h).
# Included from src/cl_dll/vgui2/sbo_vgui2.cmake (needs the VGUI2 layer and its OBJECT library sbo_vgui2_sdk).
#   SBO world: worldscreen.cpp (HUD/engine), worldscreen_bsp.cpp (bound screens: faces from the .bsp),
#              worldscreen_gl.cpp (OpenGL only), worldscreen_video.cpp (movie screens: video/ decoder and audio)
#              -> client, SBO flags + sbo_compat.h
#   SDK world: vgui2/world_screen_panel.cpp -> sbo_vgui2_sdk; worldscreen_layout_doc.cpp (plain C++: the world screen
#              editor's layout text) with it

set(SBO_WORLDSCREEN_DIR "${SBO_SRC_DIR}/cl_dll/worldscreen")

target_sources(sbo_vgui2_sdk PRIVATE ${SBO_VGUI2_DIR}/world_screen_panel.cpp ${SBO_WORLDSCREEN_DIR}/worldscreen_layout_doc.cpp)
target_include_directories(sbo_vgui2_sdk PRIVATE ${SBO_WORLDSCREEN_DIR})

set(_ws_sbo_sources ${SBO_WORLDSCREEN_DIR}/worldscreen.cpp ${SBO_WORLDSCREEN_DIR}/worldscreen_bsp.cpp
  ${SBO_WORLDSCREEN_DIR}/worldscreen_gl.cpp ${SBO_WORLDSCREEN_DIR}/worldscreen_video.cpp)
target_sources(client PRIVATE ${_ws_sbo_sources})
set_source_files_properties(${_ws_sbo_sources} PROPERTIES COMPILE_OPTIONS "${SBO_CLIENT_COMPAT_FI}")

# GL entry points are looked up at run time (worldscreen_gl.cpp). Windows: wglGetProcAddress and opengl32.dll's own
# exports (linked, so the module stays loaded; the engine loads it anyway). Linux: SDL_GL_GetProcAddress (SDL2 is
# linked already, cmake/sbo_client.cmake), no libGL dependency.
if(WIN32)
  target_link_libraries(client PRIVATE opengl32)
endif()
