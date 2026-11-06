# The updater (src/cl_dll/updater, docs/UPDATER.md). Included from cmake/sbo_client.cmake after the VGUI2 extension
# (dladdr on Linux: the client links ${CMAKE_DL_LIBS} already).
#   plain C++ (no SDK or SBO headers): updater_*.cpp + Monocypher (external/monocypher, Ed25519 verify)
#                                     -> OBJECT library sbo_updater_core, its own flags
#   SDK world: vgui2/sbo_update_http.cpp (Steam's HTTP), vgui2/sbo_update_window.cpp (dialog, cvar, commands)
#              -> sbo_vgui2_sdk
#
# Settings (one place; scripts/updater_release.py reads SBO_UPDATE_REPO's default from here):
set(SBO_UPDATE_REPO "syndicate-black-ops/syndicate-black-ops" CACHE STRING
    "GitHub repository (owner/name) whose releases the updater reads; empty: no update checks")
set(SBO_UPDATE_PUBKEY "a69945cca320a9dc327585284d9a423d93950dbbf049627311e6c999aaf964d8" CACHE STRING
    "Ed25519 public key (64 hex digits) that signs the releases' manifest.json; empty: notice only, no install")
option(SBO_UPDATE_PRERELEASES "The updater also offers GitHub pre-releases (test builds only)" OFF)

set(SBO_UPDATER_DIR "${SBO_SRC_DIR}/cl_dll/updater")
set(SBO_MONOCYPHER_DIR "${SBO_SRC_DIR}/../external/monocypher")
get_filename_component(SBO_MONOCYPHER_DIR "${SBO_MONOCYPHER_DIR}" ABSOLUTE)

add_library(sbo_updater_core OBJECT
  ${SBO_UPDATER_DIR}/updater_json.cpp
  ${SBO_UPDATER_DIR}/updater_core.cpp
  ${SBO_UPDATER_DIR}/updater_sha256.cpp
  ${SBO_UPDATER_DIR}/updater_crypto.cpp
  ${SBO_UPDATER_DIR}/updater_install.cpp
  ${SBO_UPDATER_DIR}/updater_client.cpp
  ${SBO_MONOCYPHER_DIR}/monocypher.c
  ${SBO_MONOCYPHER_DIR}/monocypher-ed25519.c)
target_include_directories(sbo_updater_core PRIVATE ${SBO_UPDATER_DIR} ${SBO_MONOCYPHER_DIR} ${SBO_SRC_DIR}/cl_dll/vgui2)
set_target_properties(sbo_updater_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(WIN32)
  target_compile_definitions(sbo_updater_core PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
  target_compile_options(sbo_updater_core PRIVATE /EHsc /Zi)
else()
  target_compile_options(sbo_updater_core PRIVATE -fPIC -fvisibility=hidden -fno-gnu-unique)
endif()
target_sources(client PRIVATE $<TARGET_OBJECTS:sbo_updater_core>)

target_sources(sbo_vgui2_sdk PRIVATE ${SBO_VGUI2_DIR}/sbo_update_http.cpp ${SBO_VGUI2_DIR}/sbo_update_window.cpp)
set_source_files_properties(${SBO_VGUI2_DIR}/sbo_update_window.cpp PROPERTIES COMPILE_DEFINITIONS
  "SBO_UPDATE_REPO=\"${SBO_UPDATE_REPO}\";SBO_UPDATE_PUBKEY=\"${SBO_UPDATE_PUBKEY}\";SBO_UPDATE_PRERELEASES=$<BOOL:${SBO_UPDATE_PRERELEASES}>")
