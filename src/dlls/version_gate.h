//=========================================================
// SBO 2.0 version gate: see version_gate.cpp and docs/VERSION_GATE.md.
//=========================================================

#ifndef VERSION_GATE_H
#define VERSION_GATE_H

// Called first in ClientConnect. FALSE: refuse the client, szRejectReason holds the one-line reason.
BOOL VersionGate_ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] );

#endif // VERSION_GATE_H
