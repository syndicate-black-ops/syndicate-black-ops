// Engine shim: the in-game particle editor (src/cl_dll/aurora/aurora_editor.h) is not part of the web editor core;
// the manager's call to its preview does nothing here.
#pragma once

inline void AuroraEditor_Frame( void ) {}
