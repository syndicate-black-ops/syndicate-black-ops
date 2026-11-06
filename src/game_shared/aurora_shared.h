// SBO 2.0: the "Aurora" user message (env_particle -> client Aurora particle systems), shared by the server
// (dlls/env_particle.cpp) and the client (cl_dll/aurora/particlemgr.cpp). docs/AURORA.md.
//
//   byte    flags (AURORA_MSG_*)
//   short   id: the env_particle's entity index; 0 for a one-shot copy (Spawn Use)
//   -- AURORA_MSG_REMOVE: nothing more
//   long x3 origin (float bits): the emitter, or with AURORA_MSG_FOLLOW its offset in the target's frame
//   coord x3 angles: the spray's turn, or with AURORA_MSG_FOLLOW the turn added to the target's angles
//   -- AURORA_MSG_FOLLOW:
//   short   the target's entity index
//   char    the target's attachment 0 .. 3, -1 = its origin
//   --
//   string  the script (aurora/<name>.aur), at most AURORA_MAX_SCRIPT - 1 characters
//
// At most 1 + 2 + 12 + 6 + 3 + 64 = 88 bytes, under the 192 bytes of a user message.
#pragma once

#define AURORA_MSG_ON		1	// emitting
#define AURORA_MSG_ONESHOT	2	// a copy made by Spawn Use: emits for its main type's lifetime, then goes
#define AURORA_MSG_REMOVE	4	// the entity was removed
#define AURORA_MSG_FOLLOW	8	// placed on another entity

#define AURORA_MAX_SCRIPT	64	// with the 0; also the engine's limit for a file in the download list
