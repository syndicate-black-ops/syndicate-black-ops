//=========================================================
// SBO 2.0 version gate, shared by the client and the server (docs/VERSION_GATE.md).
//
// The client registers the userinfo cvar VERSION_GATE_KEY with its game version ("2.1.0", the x.y.z of
// sbo_version_gen.h, without the build part) as soon as client.dll starts (CHud::Init). The engine sends the userinfo
// in the connect packet, so the server's ClientConnect reads it before anything is downloaded and refuses a client
// without it or with an older version (src/dlls/version_gate.cpp). The leading '_' keeps the key out of the copies of
// the userinfo the engine sends to the other clients. Not archived: every start sets it from the build again.
//=========================================================

#ifndef VERSION_GATE_SHARED_H
#define VERSION_GATE_SHARED_H

#define VERSION_GATE_KEY	"_cl_version"

#endif // VERSION_GATE_SHARED_H
