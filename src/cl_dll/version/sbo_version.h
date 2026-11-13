//========= SBO: build version =========//
//
// Purpose: The game version and where the build came from, for the Advanced options "Version" tab (and anything
//          else that wants to print it). Values come from the header cmake/sbo_version.cmake generates on every
//          build:
//            CI build:     "2.1.0+1a2b3c4d"            (CI_COMMIT_SHORT_SHA)
//            local build:  "2.1.0+dirty@HEAD-1a2b3c4d" (not a CI build)
//          SboVersion_Full() adds the build date like studiomdlPlusPlus's banner: "2.1.0+1a2b3c4d (Oct 10 2026 17:02:11)".
//
// Plain C++ (no SDK or SBO headers): usable from both the SBO and the VGUI2 (hl1_source_sdk) sources.
//======================================//

#ifndef SBO_VERSION_H
#define SBO_VERSION_H

//! "2.1.0"
const char *SboVersion_Number();
//! "2.1.0+1a2b3c4d" or "2.1.0+dirty@HEAD-1a2b3c4d"
const char *SboVersion_String();
//! "Oct 10 2026 17:02:11" (UTC)
const char *SboVersion_BuildDate();
//! "2.1.0+1a2b3c4d (Oct 10 2026 17:02:11)"
const char *SboVersion_Full();
//! True for CI builds.
bool SboVersion_IsCI();

#endif // SBO_VERSION_H
