# Client target (included from the top-level CMakeLists.txt): client.dll on Windows, client.so on Linux
# (docs/dev/LINUX_CLIENT.md).
#
# Source list = the "cl_dll - Win32" file list of SBO 1.38's cl_dll.dsp (legacy branch; same order, overview.cpp
# excluded as in the .dsp; hud_fallingpart.cpp removed in 2.0, see docs/WEATHER.md). Add new client files here.
#
# Extending:
#  * add sources/includes/libs/defines with the cache/list variables below, or
#  * call target_sources()/target_link_libraries() on target `client` from a file listed in SBO_CLIENT_EXTENSIONS
#    (or from the top-level CMakeLists.txt after this file is included).
#  * The VGUI2 UI (src/cl_dll/vgui2, hl1_source_sdk) is always built: sbo_vgui2.cmake is the default extension.
#    SBO 2.0 has no VGUI1-only client any more (the SBO_ENABLE_VGUI2 option and the SBO_VGUI2 define are gone).

set(SBO_CLIENT_SOURCES
  ${SBO_SRC_DIR}/dlls/444.cpp
  ${SBO_SRC_DIR}/dlls/ben.cpp
  ${SBO_SRC_DIR}/dlls/canon.cpp
  ${SBO_SRC_DIR}/dlls/chaingun.cpp
  ${SBO_SRC_DIR}/cl_dll/ev_hldm.cpp
  ${SBO_SRC_DIR}/dlls/fa18.cpp
  ${SBO_SRC_DIR}/dlls/federal.cpp
  ${SBO_SRC_DIR}/dlls/fistoflegend.cpp
  ${SBO_SRC_DIR}/dlls/glauncher.cpp
  ${SBO_SRC_DIR}/dlls/handgrenade.cpp
  ${SBO_SRC_DIR}/cl_dll/hl/hl_baseentity.cpp
  ${SBO_SRC_DIR}/cl_dll/hl/hl_events.cpp
  ${SBO_SRC_DIR}/cl_dll/hl/hl_objects.cpp
  ${SBO_SRC_DIR}/cl_dll/hl/hl_weapons.cpp
  ${SBO_SRC_DIR}/common/interface.cpp
  ${SBO_SRC_DIR}/dlls/medikit.cpp
  ${SBO_SRC_DIR}/dlls/nm33.cpp
  ${SBO_SRC_DIR}/dlls/pm66.cpp
  ${SBO_SRC_DIR}/dlls/rc28.cpp
  ${SBO_SRC_DIR}/dlls/rpg.cpp
  ${SBO_SRC_DIR}/dlls/satchel.cpp
  ${SBO_SRC_DIR}/cl_dll/sniper.cpp
  ${SBO_SRC_DIR}/dlls/spa.cpp
  ${SBO_SRC_DIR}/dlls/squeakgrenade.cpp
  ${SBO_SRC_DIR}/dlls/sw2.cpp
  ${SBO_SRC_DIR}/dlls/tripmine.cpp
  ${SBO_SRC_DIR}/game_shared/voice_banmgr.cpp
  ${SBO_SRC_DIR}/game_shared/voice_status.cpp
  ${SBO_SRC_DIR}/cl_dll/ammo.cpp
  ${SBO_SRC_DIR}/cl_dll/ammo_secondary.cpp
  ${SBO_SRC_DIR}/cl_dll/ammohistory.cpp
  ${SBO_SRC_DIR}/cl_dll/battery.cpp
  ${SBO_SRC_DIR}/cl_dll/cdll_int.cpp
  ${SBO_SRC_DIR}/cl_dll/com_weapons.cpp
  ${SBO_SRC_DIR}/cl_dll/death.cpp
  ${SBO_SRC_DIR}/cl_dll/demo.cpp
  ${SBO_SRC_DIR}/cl_dll/entity.cpp
  ${SBO_SRC_DIR}/cl_dll/movewith_client.cpp
  ${SBO_SRC_DIR}/cl_dll/ev_common.cpp
  ${SBO_SRC_DIR}/cl_dll/events.cpp
  ${SBO_SRC_DIR}/cl_dll/flashlight.cpp
  ${SBO_SRC_DIR}/cl_dll/GameStudioModelRenderer.cpp
  ${SBO_SRC_DIR}/cl_dll/geiger.cpp
  ${SBO_SRC_DIR}/cl_dll/health.cpp
  ${SBO_SRC_DIR}/cl_dll/hud.cpp
  ${SBO_SRC_DIR}/cl_dll/hud_flamme.cpp
  ${SBO_SRC_DIR}/cl_dll/hud_fumee.cpp
  ${SBO_SRC_DIR}/cl_dll/hud_msg.cpp
  ${SBO_SRC_DIR}/cl_dll/hud_redraw.cpp
  ${SBO_SRC_DIR}/cl_dll/hud_spectator.cpp
  ${SBO_SRC_DIR}/cl_dll/hud_update.cpp
  ${SBO_SRC_DIR}/cl_dll/in_camera.cpp
  ${SBO_SRC_DIR}/cl_dll/input.cpp
  ${SBO_SRC_DIR}/cl_dll/inputw32.cpp
  ${SBO_SRC_DIR}/cl_dll/interpolation.cpp
  ${SBO_SRC_DIR}/cl_dll/menu.cpp
  ${SBO_SRC_DIR}/cl_dll/message.cpp
  ${SBO_SRC_DIR}/cl_dll/mirror.cpp
  ${SBO_SRC_DIR}/cl_dll/mirror_gl.cpp
  ${SBO_SRC_DIR}/cl_dll/overview_gl.cpp
  ${SBO_SRC_DIR}/cl_dll/overlay.cpp
  ${SBO_SRC_DIR}/cl_dll/colorcorrection.cpp
  ${SBO_SRC_DIR}/cl_dll/colorcorrection_core.cpp
  ${SBO_SRC_DIR}/cl_dll/colorcorrection_gl.cpp
  ${SBO_SRC_DIR}/cl_dll/colorcorrection_ops.cpp
  ${SBO_SRC_DIR}/cl_dll/overlay_core.cpp
  ${SBO_SRC_DIR}/cl_dll/objective_core.cpp
  ${SBO_SRC_DIR}/cl_dll/overlay_gl.cpp
  ${SBO_SRC_DIR}/cl_dll/inventory.cpp
  ${SBO_SRC_DIR}/cl_dll/inventory_core.cpp
  ${SBO_SRC_DIR}/cl_dll/weapon_hand.cpp
  ${SBO_SRC_DIR}/cl_dll/render_fade.cpp
  ${SBO_SRC_DIR}/cl_dll/head_scale.cpp
  ${SBO_SRC_DIR}/cl_dll/killfeed_icons.cpp
  ${SBO_SRC_DIR}/cl_dll/objecthint.cpp
  ${SBO_SRC_DIR}/cl_dll/break_gibs.cpp
  ${SBO_SRC_DIR}/cl_dll/dlights.cpp
  ${SBO_SRC_DIR}/cl_dll/light_origin.cpp
  ${SBO_SRC_DIR}/cl_dll/static_props.cpp
  ${SBO_SRC_DIR}/cl_dll/server_menu_preview.cpp
  ${SBO_SRC_DIR}/cl_dll/server_menu_preview_gl.cpp
  ${SBO_SRC_DIR}/cl_dll/map_music.cpp
  ${SBO_SRC_DIR}/cl_dll/mover_prediction.cpp
  ${SBO_SRC_DIR}/cl_dll/parsemsg.cpp
  ${SBO_SRC_DIR}/cl_dll/particules.cpp
  ${SBO_SRC_DIR}/pm_shared/pm_debug.c
  ${SBO_SRC_DIR}/pm_shared/pm_materials.c
  ${SBO_SRC_DIR}/pm_shared/pm_math.c
  ${SBO_SRC_DIR}/pm_shared/pm_shared.c
  ${SBO_SRC_DIR}/cl_dll/saytext.cpp
  ${SBO_SRC_DIR}/cl_dll/status_icons.cpp
  ${SBO_SRC_DIR}/cl_dll/statusbar.cpp
  ${SBO_SRC_DIR}/cl_dll/studio_util.cpp
  ${SBO_SRC_DIR}/cl_dll/studio_fullbright_gl.cpp
  ${SBO_SRC_DIR}/cl_dll/StudioModelRenderer.cpp
  ${SBO_SRC_DIR}/cl_dll/text_message.cpp
  ${SBO_SRC_DIR}/cl_dll/train.cpp
  ${SBO_SRC_DIR}/cl_dll/tri.cpp
  ${SBO_SRC_DIR}/cl_dll/util.cpp
  ${SBO_SRC_DIR}/cl_dll/spectator_panel.cpp
  ${SBO_SRC_DIR}/cl_dll/viewport.cpp
  ${SBO_SRC_DIR}/cl_dll/view.cpp
  # SBO 2.0
  ${SBO_SRC_DIR}/cl_dll/hud_scale.cpp
  ${SBO_SRC_DIR}/cl_dll/discord/discord_ipc.cpp
  ${SBO_SRC_DIR}/cl_dll/discord/discord_rpc.cpp
  ${SBO_SRC_DIR}/cl_dll/version/sbo_version.cpp
  # CS 1.6 weather (cs16-client, docs/WEATHER.md)
  ${SBO_SRC_DIR}/cl_dll/environment.cpp
  ${SBO_SRC_DIR}/cl_dll/materials.cpp
  ${SBO_SRC_DIR}/cl_dll/particleman/CBaseParticle.cpp
  ${SBO_SRC_DIR}/cl_dll/particleman/CFrustum.cpp
  ${SBO_SRC_DIR}/cl_dll/particleman/CMiniMem.cpp
  ${SBO_SRC_DIR}/cl_dll/particleman/IParticleMan_Active.cpp
  # Spirit of Half-Life's Aurora particle systems (env_particle, docs/AURORA.md)
  ${SBO_SRC_DIR}/cl_dll/aurora/particlesys.cpp
  ${SBO_SRC_DIR}/cl_dll/aurora/particlemgr.cpp
  # the in-game particle editor's preview and script document (particleeditorui, docs/AURORA.md "Editor")
  ${SBO_SRC_DIR}/cl_dll/aurora/aurora_editor.cpp
  ${SBO_SRC_DIR}/cl_dll/aurora/aurora_doc.cpp
  # point_worldtext (docs/POINT_WORLDTEXT.md)
  ${SBO_SRC_DIR}/cl_dll/worldtext.cpp
  ${SBO_SRC_DIR}/cl_dll/worldtext_core.cpp
  ${SBO_SRC_DIR}/cl_dll/worldtext_gl.cpp
  # animated sprays (docs/ANIMATED_SPRAYS.md)
  ${SBO_SRC_DIR}/cl_dll/spray_anim.cpp
  ${SBO_SRC_DIR}/cl_dll/spray_anim_core.cpp
)

# Discord Rich Presence (src/cl_dll/discord): the application ID from the Discord developer portal; empty = off.
# The updater's settings (SBO_UPDATE_REPO, SBO_UPDATE_PUBKEY, SBO_UPDATE_PRERELEASES): src/cl_dll/updater/updater.cmake
set(SBO_DISCORD_APP_ID "1558528241294123178" CACHE STRING "Discord application ID for Rich Presence (empty: disabled)")

# The VGUI2 client UI (src/cl_dll/vgui2, docs/dev/CHAT_PORT_INTEGRATION.md) is part of every build.
set(SBO_CLIENT_EXTENSIONS "${SBO_SRC_DIR}/cl_dll/vgui2/sbo_vgui2.cmake" CACHE STRING
    "CMake files included after the client target is defined")
set(SBO_CLIENT_EXTRA_SOURCES "" CACHE STRING "Extra client.dll sources (;-list)")
set(SBO_CLIENT_EXTRA_INCLUDES "" CACHE STRING "Extra client.dll include dirs (;-list)")
set(SBO_CLIENT_EXTRA_DEFINES "" CACHE STRING "Extra client.dll preprocessor defines (;-list)")
set(SBO_CLIENT_EXTRA_LIBS "" CACHE STRING "Extra client.dll link libraries (;-list, paths or targets)")

add_library(client SHARED ${SBO_CLIENT_SOURCES} ${SBO_CLIENT_EXTRA_SOURCES})
set_target_properties(client PROPERTIES
  OUTPUT_NAME client
  PREFIX ""
  # the VC6 build's client.lib/client.exp are useless to the engine; keep the import lib out of the way
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/implib")

# Include order of cl_dll.dsp ("Win32 Debug": dlls, common, pm_shared, engine, vgui, game_shared). cl_dll itself
# comes first implicitly in VC6 (directory of the including file); add it explicitly for files outside cl_dll.
target_include_directories(client PRIVATE
  ${SBO_SRC_DIR}/cl_dll
  ${SBO_SRC_DIR}/cl_dll/particleman
  ${SBO_SRC_DIR}/dlls
  ${SBO_SRC_DIR}/common
  ${SBO_SRC_DIR}/pm_shared
  ${SBO_SRC_DIR}/engine
  ${SBO_SRC_DIR}/game_shared
  ${SBO_CLIENT_EXTRA_INCLUDES})

target_compile_definitions(client PRIVATE
  CLIENT_DLL CLIENT_WEAPONS
  SBO_DISCORD_APP_ID="${SBO_DISCORD_APP_ID}"
  ${SBO_CLIENT_EXTRA_DEFINES})
if(SBO_TEST_COMMANDS)
  # test builds only: the client's test_cl_* commands (test_cl_worldtext: cl_dll/worldtext.cpp)
  target_compile_definitions(client PRIVATE SBO_TEST_COMMANDS)
endif()

if(WIN32)
target_compile_definitions(client PRIVATE
  WIN32 _WINDOWS _MBCS
  # quiet modern-CRT deprecations for the 2003 code (strcpy, sprintf, POSIX names, ...)
  _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE _CRT_NONSTDC_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)

# /GX -> /EHsc, /GR as in cl_dll.dsp. /Zi everywhere so we get a PDB.
# /permissive: MSVC's lenient mode (clang-cl is -fms-compatibility by default). VC6 for-loop scoping was fixed in the
# source (clang-cl has no /Zc:forScope-); cl.exe gets /Zc:forScope- as a safety net.
target_compile_options(client PRIVATE /EHsc /GR /Zi /W3 $<$<COMPILE_LANGUAGE:CXX>:/permissive>)
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(client PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/Zc:forScope->)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(client PRIVATE
    # HL code type-puns floats/ints and relies on MSVC-style aliasing
    /clang:-fno-strict-aliasing
    -Wno-microsoft -Wno-writable-strings -Wno-c++11-narrowing -Wno-deprecated-declarations
    -Wno-unused-value -Wno-unused-variable -Wno-unused-private-field -Wno-unused-function
    -Wno-dangling-else -Wno-parentheses -Wno-logical-op-parentheses -Wno-bitwise-op-parentheses
    -Wno-switch -Wno-tautological-compare -Wno-tautological-constant-out-of-range-compare
    -Wno-pointer-bool-conversion -Wno-null-conversion -Wno-int-to-void-pointer-cast
    -Wno-incompatible-pointer-types -Wno-missing-braces -Wno-unknown-pragmas -Wno-ignored-pragmas
    -Wno-char-subscripts -Wno-format -Wno-format-security -Wno-comment -Wno-invalid-noreturn
    -Wno-inconsistent-missing-override -Wno-overloaded-virtual -Wno-delete-non-abstract-non-virtual-dtor
    -Wno-self-assign -Wno-misleading-indentation -Wno-implicit-function-declaration
    -Wno-sizeof-pointer-memaccess -Wno-string-plus-int -Wno-constant-conversion -Wno-macro-redefined
    # C tentative definitions are COMDAT-like "common" symbols in MSVC (pm_shared.c's vJumpOrigin/vJumpAngles are
    # also defined in view.cpp); clang defaults to -fno-common
    $<$<COMPILE_LANGUAGE:C>:/clang:-fcommon>
    /clang:-ferror-limit=0)
endif()

# sbo_compat.h: <windows.h> with its HSPRITE handle renamed (see the header). Only for SBO's own sources, so code
# added through SBO_CLIENT_EXTRA_SOURCES / extensions (hl1_source_sdk, vgui2) is compiled without it.
# New sources that include SBO's HUD/engine headers (HSPRITE) AND <windows.h> need it too:
#   set_source_files_properties(<files> PROPERTIES COMPILE_OPTIONS "${SBO_CLIENT_COMPAT_FI}")
set(SBO_CLIENT_COMPAT_FI "/FI${CMAKE_CURRENT_LIST_DIR}/compat/sbo_compat.h")
set_source_files_properties(${SBO_CLIENT_SOURCES} PROPERTIES COMPILE_OPTIONS "${SBO_CLIENT_COMPAT_FI}")

# Linker inputs of cl_dll.dsp: winmm, wsock32. SBO 2.0 has no VGUI1 code: VGUI1's vgui.lib (the import lib of the
# engine's vgui.dll) and its headers (the SDK's utils/vgui, not in this tree) are not used any more.
# (oleaut32/uuid/odbc32/odbccp32/winspool/comdlg32/gdi32/shell32/ole32/advapi32 were wizard defaults, unused.)
target_link_libraries(client PRIVATE
  winmm wsock32 user32 kernel32 winhttp advapi32
  ${SBO_CLIENT_EXTRA_LIBS})
target_link_options(client PRIVATE /DEBUG /MAP /MANIFEST:NO /INCREMENTAL:NO
  $<$<NOT:$<CONFIG:Debug>>:/OPT:REF /OPT:ICF>)

else()
# Linux: client.so for Valve's hl_linux (cl_dlls/client.so), docs/dev/LINUX_CLIENT.md. Flags follow the SDK's
# linux/Makefile (BASE_CFLAGS, -fvisibility=hidden, -fno-gnu-unique), BugfixedHL-Rebased and the Linux server above.
target_compile_definitions(client PRIVATE
  LINUX _LINUX POSIX _POSIX GNUC
  # the SDK's Makefile.hl_cdll: pm_math.c's vec3_origin would clash with cl_dll/util.cpp's (no C++ name mangling
  # for variables in the Itanium ABI)
  DISABLE_VEC_ORIGIN
  # C runtime names of the Windows code (the SDK's Makefile; _snwprintf: Makefile.hl_cdll)
  stricmp=strcasecmp _stricmp=strcasecmp strnicmp=strncasecmp _strnicmp=strncasecmp
  _snprintf=snprintf _vsnprintf=vsnprintf _alloca=alloca _snwprintf=swprintf)

# -fvisibility=hidden: only the functions marked __declspec( dllexport ) (sbo_compat.h) are exported.
# -fcommon: pm_shared.c's vJumpOrigin/vJumpAngles tentative definitions (see the clang-cl note above).
set(SBO_CLIENT_GNU_FLAGS -fPIC -fvisibility=hidden -fno-gnu-unique -fno-strict-aliasing)
target_compile_options(client PRIVATE ${SBO_CLIENT_GNU_FLAGS} -fcommon -w
  $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)

# sbo_compat.h on Linux: the <windows.h> types and MSVC keywords the client code uses (see the header). A target
# option here, not a source file property as on Windows: the weapon and pm_shared sources are also the server's, and
# hl.so is built without it. (The SDK-world VGUI2 sources are their own target, sbo_vgui2_sdk.)
set(SBO_CLIENT_COMPAT_FI "")
target_compile_options(client PRIVATE "SHELL:-include ${CMAKE_CURRENT_LIST_DIR}/compat/sbo_compat.h")

# SDL2: mouse input (inputw32.cpp), as in the SDK and BugfixedHL-Rebased. The headers and the link-time library are
# the toolchain image's (Steam Runtime); at run time the engine's own libSDL2-2.0.so.0 is already loaded.
find_path(SBO_SDL2_INCLUDE_DIR SDL2/SDL_mouse.h REQUIRED)
find_library(SBO_SDL2_LIBRARY NAMES SDL2-2.0 SDL2 REQUIRED)
target_include_directories(client PRIVATE ${SBO_SDL2_INCLUDE_DIR})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(client PRIVATE
  ${SBO_SDL2_LIBRARY} Threads::Threads m
  ${SBO_CLIENT_EXTRA_LIBS})
# libstdc++/libgcc static like the Linux server: hl_linux ships its own old libstdc++.so.6. --exclude-libs keeps
# their symbols out of client.so's dynamic symbol table (the engine and its libraries have their own copies).
target_link_options(client PRIVATE -static-libstdc++ -static-libgcc -Wl,--no-undefined
  -Wl,--exclude-libs,libstdc++.a -Wl,--exclude-libs,libgcc.a -Wl,--exclude-libs,libgcc_eh.a
  -Wl,--version-script=${CMAKE_CURRENT_LIST_DIR}/client.version
  -Wl,-Map=${CMAKE_BINARY_DIR}/client.map)
set_property(TARGET client APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/client.version)
# no RPATH to the build tree's import libraries: the engine's libraries are found through hl.sh's LD_LIBRARY_PATH
set_target_properties(client PROPERTIES SKIP_BUILD_RPATH ON)
endif()

# Build version header (sbo_version_gen.h, the sbo_version_gen target in the top-level CMakeLists.txt).
add_dependencies(client sbo_version_gen)
target_include_directories(client PRIVATE ${SBO_GENERATED_DIR})

# game_playvideo video player (docs/VIDEO_PLAYER.md): its sources and the vendored decoders
include(${SBO_SRC_DIR}/cl_dll/video/video.cmake)

foreach(_ext IN LISTS SBO_CLIENT_EXTENSIONS)
  include("${_ext}")
endforeach()
# the updater (docs/UPDATER.md): after the VGUI2 extension (its dialog and HTTP are in sbo_vgui2_sdk)
include(${SBO_SRC_DIR}/cl_dll/updater/updater.cmake)
if(NOT WIN32)
  # after the extensions' static libraries (tier1's interface.cpp: dlopen/dlsym)
  target_link_libraries(client PRIVATE ${CMAKE_DL_LIBS})
endif()

install(TARGETS client RUNTIME DESTINATION . LIBRARY DESTINATION .)
if(WIN32)
  install(FILES $<TARGET_PDB_FILE:client> DESTINATION . OPTIONAL)
endif()
