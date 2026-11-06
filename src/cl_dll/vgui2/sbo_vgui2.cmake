# VGUI2 client UI -- client.dll / client.so extension.
#
# Used by cmake/sbo_client.cmake (SBO_CLIENT_EXTENSIONS=<this file>, always; included after target `client` is
# defined).
#
# Two "header worlds" (see docs/dev/CHAT_PORT_INTEGRATION.md):
#   * SDK world  (hl1_source_sdk includes/defines): client_vgui.cpp, client_viewport.cpp, hud_chat.cpp,
#                hud_chat_color_picker.cpp, score_panel.cpp, hud_deathnotice.cpp, hud_voice_status.cpp,
#                avatar_image.cpp, tga_image.cpp, client_steam_context.cpp
#                sbo_options.cpp, sbo_menus.cpp, sbo_widgets.cpp, sbo_text_window.cpp, sbo_html_panel.cpp,
#                sbo_gm_menu.cpp,
#                sbo_team_menu.cpp, sbo_brief_menu.cpp, sbo_loadout_menu.cpp, sbo_brief_item.cpp,
#                sbo_spectator_panel.cpp, sbo_spectator_info.cpp, tutor_text_window.cpp, sbo_hud_menu.cpp,
#                sbo_command_menu_panel.cpp, sbo_message_box.cpp, sbo_console.cpp, sbo_server_browser.cpp,
#                sbo_server_address.cpp, sbo_image_decode.cpp, sbo_a2s_rules.cpp (no SDK headers; in this list for its flags only),
#                sbo_ui_style.cpp, sbo_map_picture.cpp, sbo_loading_screen.cpp, sbo_script_file.cpp (no SDK
#                headers), sbo_settings_window.cpp, sbo_options_window.cpp, sbo_create_server.cpp,
#                sbo_server_menu.cpp, sbo_server_menu_proto.cpp (no SDK headers),
#                sbo_gm_spawn_menu.cpp
#                -> OBJECT library sbo_vgui2_sdk (an OBJECT library, not STATIC: client_vgui.obj is only
#                   reachable through the ClientFactory dllexport and the EXPOSE_SINGLE_INTERFACE static
#                   registrar, so a static-lib member would be dropped by the linker)
#   * SBO world  (client's own includes + sbo_compat.h): sbo_bridge.cpp, sbo_killfeed_bridge.cpp,
#                sbo_menu_bridge.cpp, sbo_command_menu.cpp, sbo_text_scheme.cpp, sbo_server_menu_bridge.cpp
#                -> added to `client` directly

set(SBO_VGUI2_DIR "${SBO_SRC_DIR}/cl_dll/vgui2")
set(SBO_SOURCE_SDK_DIR "${CMAKE_SOURCE_DIR}/external/hl1_source_sdk")

if(NOT EXISTS "${SBO_SOURCE_SDK_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "hl1_source_sdk submodule missing: git submodule update --init external/hl1_source_sdk")
endif()

# hl1_source_sdk options (same as BugfixedHL-Rebased)
set(SOURCE_SDK_GFX_PATH "ui/gfx/vgui")   # 800corner*.tga for rounded panel backgrounds
# Steam API for the scoreboard avatars / profile overlay (BHL's setup): link steam_api_min.lib, i.e. only the
# steam_api.dll exports every Half-Life engine build ships (steam_api.dll sits next to hl.exe);
# SteamAPI_(Un)RegisterCallResult are looked up at run time (tier2/steam_api.cpp).
set(SOURCE_SDK_MIN_STEAM_API TRUE)
# NO_VCR: GoldSrc's tier0.dll (HL25) does not export g_pVCR (Source demo-recording hooks); without this the SDK's
# CThreadMutex::Lock imports it and client.dll fails to load (STATUS_ENTRYPOINT_NOT_FOUND).
add_compile_definitions(NO_VCR)
# ALIGN16 member-function pointers: MessageMapItem_t / KeyBindingMap's func are declared ALIGN16, but their
# CUtlVector storage comes from malloc (8-byte aligned on Win32). clang copies them with movaps -> #GP crash in
# Panel::Panel (AddToMap). MSVC never emits aligned moves there. Use copies of the two headers without ALIGN16,
# first on the include path for every SDK and VGUI2 source (same layout everywhere; never crosses the engine).
set(SBO_SDK_OVERRIDE_DIR "${CMAKE_BINARY_DIR}/sdk_overrides")
foreach(_hdr MessageMap.h KeyBindingMap.h)
  file(READ "${SBO_SOURCE_SDK_DIR}/public/vgui_controls/${_hdr}" _txt)
  string(REPLACE "ALIGN16 MessageFunc_t" "MessageFunc_t" _txt "${_txt}")
  file(WRITE "${SBO_SDK_OVERRIDE_DIR}/vgui_controls/${_hdr}" "${_txt}")
endforeach()
include_directories(BEFORE "${SBO_SDK_OVERRIDE_DIR}")
add_subdirectory("${SBO_SOURCE_SDK_DIR}" "${CMAKE_BINARY_DIR}/hl1_source_sdk")

# clang-cl: RESTRICT mismatch in tier1/bitbuf (see sdk_clang_compat.h). /arch:SSE2 = MSVC's x86 default
# (clang-cl's i686 default has no SSE; mathlib.h uses SSE intrinsics). The engine requires SSE2 CPUs anyway.
# Linux (BugfixedHL-Rebased's flags): -msse2 -mfpmath=sse for mathlib; hidden visibility and PIC for the static
# libraries that go into client.so (SBO_CLIENT_GNU_FLAGS, cmake/sbo_client.cmake).
if(WIN32)
  set(SBO_VGUI2_SDK_FLAGS /arch:SSE2)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    list(APPEND SBO_VGUI2_SDK_FLAGS "/FI${SBO_VGUI2_DIR}/sdk_clang_compat.h")
  endif()
  set(SBO_VGUI2_NO_WARNINGS /w)
else()
  set(SBO_VGUI2_SDK_FLAGS -msse2 -mfpmath=sse ${SBO_CLIENT_GNU_FLAGS})
  set(SBO_VGUI2_NO_WARNINGS -w)
endif()
foreach(_t tier1 tier2 vgui_controls)
  # third-party code: no warnings (Source SDK switch/parentheses noise)
  target_compile_options(${_t} PRIVATE ${SBO_VGUI2_SDK_FLAGS} ${SBO_VGUI2_NO_WARNINGS})
endforeach()

# SDK-world sources
add_library(sbo_vgui2_sdk OBJECT
  ${SBO_VGUI2_DIR}/client_vgui.cpp
  ${SBO_VGUI2_DIR}/client_viewport.cpp
  ${SBO_VGUI2_DIR}/hud_chat.cpp
  ${SBO_VGUI2_DIR}/hud_chat_color_picker.cpp
  ${SBO_VGUI2_DIR}/sbo_console.cpp
  ${SBO_VGUI2_DIR}/score_panel.cpp
  ${SBO_VGUI2_DIR}/hud_deathnotice.cpp
  ${SBO_VGUI2_DIR}/hud_voice_status.cpp
  ${SBO_VGUI2_DIR}/avatar_image.cpp
  ${SBO_VGUI2_DIR}/tga_image.cpp
  ${SBO_VGUI2_DIR}/client_steam_context.cpp
  ${SBO_VGUI2_DIR}/sbo_options.cpp
  ${SBO_VGUI2_DIR}/sbo_menus.cpp
  ${SBO_VGUI2_DIR}/sbo_widgets.cpp
  ${SBO_VGUI2_DIR}/sbo_text_window.cpp
  ${SBO_VGUI2_DIR}/sbo_html_panel.cpp
  ${SBO_VGUI2_DIR}/sbo_gm_hud.cpp
  ${SBO_VGUI2_DIR}/sbo_gm_menu.cpp
  ${SBO_VGUI2_DIR}/sbo_team_menu.cpp
  ${SBO_VGUI2_DIR}/sbo_brief_menu.cpp
  ${SBO_VGUI2_DIR}/sbo_loadout_menu.cpp
  ${SBO_VGUI2_DIR}/sbo_brief_item.cpp
  ${SBO_VGUI2_DIR}/sbo_spectator_panel.cpp
  ${SBO_VGUI2_DIR}/sbo_spectator_info.cpp
  ${SBO_VGUI2_DIR}/tutor_text_window.cpp
  ${SBO_VGUI2_DIR}/sbo_hud_menu.cpp
  ${SBO_VGUI2_DIR}/sbo_command_menu_panel.cpp
  ${SBO_VGUI2_DIR}/sbo_message_box.cpp
  ${SBO_VGUI2_DIR}/sbo_server_browser.cpp
  ${SBO_VGUI2_DIR}/sbo_server_address.cpp
  ${SBO_VGUI2_DIR}/sbo_image_decode.cpp
  ${SBO_VGUI2_DIR}/sbo_a2s_rules.cpp
  ${SBO_VGUI2_DIR}/sbo_ui_style.cpp
  ${SBO_VGUI2_DIR}/sbo_map_picture.cpp
  ${SBO_VGUI2_DIR}/sbo_loading_screen.cpp
  ${SBO_VGUI2_DIR}/sbo_script_file.cpp
  ${SBO_VGUI2_DIR}/sbo_settings_window.cpp
  ${SBO_VGUI2_DIR}/sbo_options_window.cpp
  ${SBO_VGUI2_DIR}/sbo_create_server.cpp
  ${SBO_VGUI2_DIR}/sbo_server_menu.cpp
  ${SBO_VGUI2_DIR}/sbo_server_menu_proto.cpp
  ${SBO_VGUI2_DIR}/sbo_gm_spawn_menu.cpp)
target_include_directories(sbo_vgui2_sdk PRIVATE ${SBO_VGUI2_DIR} ${SOURCE_SDK_INCLUDE_PATHS})
# VERSION_SAFE_STEAM_API_INTERFACES: SteamAPI_InitSafe + CSteamAPIContext (BHL client), no SteamFriends() etc. imports
target_compile_definitions(sbo_vgui2_sdk PRIVATE ${SOURCE_SDK_DEFINES} CLIENT_DLL
  VERSION_SAFE_STEAM_API_INTERFACES _CRT_SECURE_NO_WARNINGS)
if(WIN32)
  target_compile_options(sbo_vgui2_sdk PRIVATE /EHsc /GR /Zi ${SBO_VGUI2_SDK_FLAGS})
else()
  # C runtime names of the Windows code, BugfixedHL-Rebased's COMMON_DEFINES
  target_compile_definitions(sbo_vgui2_sdk PRIVATE
    _stricmp=strcasecmp _strnicmp=strncasecmp _snprintf=snprintf _vsnprintf=vsnprintf _alloca=alloca
    _snwprintf=swprintf)
  target_compile_options(sbo_vgui2_sdk PRIVATE ${SBO_VGUI2_SDK_FLAGS} -w)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # DECLARE_CLASS_SIMPLE expands virtual overrides without `override` next to ones that have it
  target_compile_options(sbo_vgui2_sdk PRIVATE -Wno-inconsistent-missing-override)
endif()
if(SBO_TEST_COMMANDS)
  # test builds only: test_cl_devicon (score_panel.cpp)
  target_compile_definitions(sbo_vgui2_sdk PRIVATE SBO_TEST_COMMANDS)
endif()
target_sources(client PRIVATE $<TARGET_OBJECTS:sbo_vgui2_sdk>)

# SBO-world bridge: same flags as SBO's own sources (incl. the sbo_compat.h force-include)
target_sources(client PRIVATE ${SBO_VGUI2_DIR}/sbo_bridge.cpp ${SBO_VGUI2_DIR}/sbo_killfeed_bridge.cpp
  ${SBO_VGUI2_DIR}/sbo_menu_bridge.cpp ${SBO_VGUI2_DIR}/sbo_command_menu.cpp ${SBO_VGUI2_DIR}/sbo_text_scheme.cpp
  ${SBO_VGUI2_DIR}/sbo_server_menu_bridge.cpp)
set_source_files_properties(${SBO_VGUI2_DIR}/sbo_bridge.cpp ${SBO_VGUI2_DIR}/sbo_killfeed_bridge.cpp
  ${SBO_VGUI2_DIR}/sbo_menu_bridge.cpp ${SBO_VGUI2_DIR}/sbo_command_menu.cpp ${SBO_VGUI2_DIR}/sbo_text_scheme.cpp
  ${SBO_VGUI2_DIR}/sbo_server_menu_bridge.cpp
  PROPERTIES COMPILE_OPTIONS "${SBO_CLIENT_COMPAT_FI}")

# game_timer HUD timer (docs/GAME_TIMER.md): hud_game_timer.cpp is SDK world, sbo_game_timer_bridge.cpp SBO world
target_sources(sbo_vgui2_sdk PRIVATE ${SBO_VGUI2_DIR}/hud_game_timer.cpp)
target_sources(client PRIVATE ${SBO_VGUI2_DIR}/sbo_game_timer_bridge.cpp)
set_source_files_properties(${SBO_VGUI2_DIR}/sbo_game_timer_bridge.cpp
  PROPERTIES COMPILE_OPTIONS "${SBO_CLIENT_COMPAT_FI}")

# SBO's SDK 2.3 interface.cpp defines CreateInterface/InterfaceReg like tier1's interface.cpp -> drop it
set_source_files_properties(${SBO_SRC_DIR}/common/interface.cpp PROPERTIES HEADER_FILE_ONLY ON)

# vgui_controls, steam_api (steam_api_min.lib), tier2, tier1 (static) + tier0, vstdlib (import libs of the engine's DLLs)
# Linux: libsteam_api.so, libtier0.so, libvstdlib.so from the SDK's lib/public (link time); hl_linux ships them.
target_link_libraries(client PRIVATE ${SOURCE_SDK_LIBS})

# Runtime files for the mod folder (gamedir/syndicate/ in the repo)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/gamedir/syndicate/ui" DESTINATION .)

# World-space VGUI2 screens (prototype)
include("${SBO_SRC_DIR}/cl_dll/worldscreen/worldscreen.cmake")
