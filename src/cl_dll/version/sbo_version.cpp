//========= SBO: build version =========//
//
// Purpose: see sbo_version.h.
//======================================//

#include "sbo_version.h"
#include "sbo_version_gen.h" // generated in the build tree (cmake/sbo_version.cmake)

const char *SboVersion_Number() { return SBO_VERSION; }
const char *SboVersion_String() { return SBO_VERSION_STRING; }
const char *SboVersion_BuildDate() { return SBO_BUILD_DATE; }
const char *SboVersion_Full() { return SBO_VERSION_STRING " (" SBO_BUILD_DATE ")"; }
bool SboVersion_IsCI() { return SBO_VERSION_IS_CI != 0; }
