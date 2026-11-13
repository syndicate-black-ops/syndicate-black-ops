# Server target (included from the top-level CMakeLists.txt): hl.dll on Windows, hl.so on Linux.
#
# Source list = hl.dll's / syndicate.so's file set (SBO's own hl.dsp is lost; the SDK's mp.dsp list is stale).

set(SBO_SERVER_SOURCES
  444.cpp airtank.cpp ammocaisse.cpp animating.cpp animation.cpp apache.cpp barney.cpp ben.cpp bmodels.cpp br.cpp
  buttons.cpp canon.cpp cbase.cpp chaingun.cpp chainsaw.cpp change.cpp client.cpp combat.cpp coop.cpp cyborg.cpp
  dark.cpp darkgrunt.cpp defaultai.cpp dm.cpp doors.cpp effects.cpp explode.cpp fa18.cpp federal.cpp fistoflegend.cpp
  flammeentity.cpp flyingmonster.cpp fumee_entity.cpp env_particle.cpp func_break.cpp func_tank.cpp game.cpp gamerules.cpp
  gargantua.cpp genericmonster.cpp ggrenade.cpp glauncher.cpp globals.cpp gmmode.cpp grunt_hunter.cpp h_ai.cpp h_battery.cpp
  h_cine.cpp h_cycler.cpp h_export.cpp handgrenade.cpp hassassin.cpp hgrunt.cpp hunter.cpp hunter2.cpp
  ichthyosaur.cpp imp.cpp imp_hunter.cpp items.cpp lights.cpp limitator.cpp maprules.cpp maxround.cpp medikit.cpp
  meteoentity.cpp minotaure.cpp monstermaker.cpp monsters.cpp monsterstate.cpp mortar.cpp multiplay_gamerules.cpp
  nm33.cpp nodes.cpp observer.cpp osprey.cpp pathcorner.cpp plane.cpp plats.cpp player.cpp pm66.cpp rc28.cpp rpg.cpp
  satchel.cpp schedule.cpp scientist.cpp scripted.cpp semiclip.cpp skill.cpp sound.cpp soundent.cpp spa.cpp spectator.cpp
  squadmonster.cpp squeakgrenade.cpp subs.cpp sw2.cpp sword.cpp talkmonster.cpp teamplay_gamerules.cpp
  tempmonster.cpp triggers.cpp tripmine.cpp turret.cpp tutor.cpp env_tutor.cpp env_mirror.cpp point_worldtext.cpp game_messagebox.cpp env_overlay.cpp colorcorrection.cpp game_timer.cpp trigger_objective.cpp vgui_screen.cpp keyvalue.cpp baselogic.cpp trigger_changevalue.cpp trigger_condition.cpp trigger_random.cpp trigger_entity_iterator.cpp trigger_sensor.cpp featureful_triggers.cpp env_dlight.cpp light_origin.cpp locus_beam.cpp player_calc_ratio.cpp guide_effects.cpp guide_player.cpp cheat_commands.cpp guide_monsters.cpp guide_motion.cpp round_hooks.cpp locus.cpp map_compat.cpp render_fade.cpp head_scale.cpp killfeed_icons.cpp inventory.cpp objecthint.cpp state_logic.cpp alias.cpp env_static.cpp client_sendqueue.cpp client_info.cpp env_model.cpp sequence.cpp trigger_sequence.cpp movewith.cpp util.cpp vehicle.cpp version_gate.cpp warhammer.cpp
  weapons.cpp werewolf.cpp werewolf_hunter.cpp world.cpp xen.cpp zombiegrunt.cpp zombies.cpp zombies_hunter.cpp)
list(TRANSFORM SBO_SERVER_SOURCES PREPEND "${SBO_SRC_DIR}/dlls/")
list(APPEND SBO_SERVER_SOURCES
  ${SBO_SRC_DIR}/game_shared/voice_gamemgr.cpp
  ${SBO_SRC_DIR}/pm_shared/pm_debug.c
  ${SBO_SRC_DIR}/pm_shared/pm_materials.c
  ${SBO_SRC_DIR}/pm_shared/pm_math.c
  ${SBO_SRC_DIR}/pm_shared/pm_shared.c)

add_library(server SHARED ${SBO_SERVER_SOURCES})
if(SBO_TEST_COMMANDS)
  # test builds only: src/dlls/sbo_test.cpp (docs/dev/ENTITY_TESTS.md)
  target_sources(server PRIVATE ${SBO_SRC_DIR}/dlls/sbo_test.cpp)
  target_compile_definitions(server PRIVATE SBO_TEST_COMMANDS)
endif()
set_target_properties(server PROPERTIES PREFIX "" ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/implib")

target_include_directories(server PRIVATE
  ${SBO_SRC_DIR}/dlls
  ${SBO_SRC_DIR}/engine
  ${SBO_SRC_DIR}/common
  ${SBO_SRC_DIR}/pm_shared
  ${SBO_SRC_DIR}/game_shared)
target_compile_definitions(server PRIVATE VALVE_DLL CLIENT_WEAPONS)

# Build version header (sbo_version_gen.h, the sbo_version_gen target in the top-level CMakeLists.txt): game.cpp's
# game_version cvar.
add_dependencies(server sbo_version_gen)
target_include_directories(server PRIVATE ${SBO_GENERATED_DIR})

if(WIN32)
  set_target_properties(server PROPERTIES OUTPUT_NAME hl)
  target_compile_definitions(server PRIVATE WIN32 _WINDOWS
    _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE _CRT_NONSTDC_NO_WARNINGS)
  target_sources(server PRIVATE ${SBO_SRC_DIR}/dlls/hl.def)
  target_compile_options(server PRIVATE /EHsc /Zi /W3 $<$<COMPILE_LANGUAGE:CXX>:/permissive>)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(server PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/Zc:forScope->)
  else()
    target_compile_options(server PRIVATE
      /clang:-fno-strict-aliasing /clang:-ferror-limit=0 $<$<COMPILE_LANGUAGE:C>:/clang:-fcommon> -w)
  endif()
  target_link_libraries(server PRIVATE user32 advapi32)
  target_link_options(server PRIVATE /DEBUG /MAP /MANIFEST:NO /INCREMENTAL:NO
    $<$<NOT:$<CONFIG:Debug>>:/OPT:REF /OPT:ICF>)
  install(TARGETS server RUNTIME DESTINATION .)
  install(FILES $<TARGET_PDB_FILE:server> DESTINATION . OPTIONAL)
else()
  # The engine loads dlls/hl.so (liblist.gam gamedll_linux).
  set_target_properties(server PROPERTIES OUTPUT_NAME hl)
  target_compile_definitions(server PRIVATE
    stricmp=strcasecmp _strnicmp=strncasecmp strnicmp=strncasecmp)
  target_compile_options(server PRIVATE -fPIC -fno-strict-aliasing -fcommon -w
    $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)
  # No dependency on the host's libstdc++/libgcc versions; only libc/libm/libdl from the server's system.
  target_link_options(server PRIVATE -static-libstdc++ -static-libgcc -Wl,--no-undefined
    -Wl,-Map=${CMAKE_BINARY_DIR}/hl.map)
  target_link_libraries(server PRIVATE dl m)
  install(TARGETS server LIBRARY DESTINATION .)
endif()
