//=========================================================
// SBO 2.0 object hints (Featureful, docs/OBJECT_HINTS.md): the "ObjectHint" user message, server dlls/objecthint.cpp,
// client cl_dll/objecthint.cpp. The server sends each living player the hints he sees, unreliably, every
// OBJECTHINT_INTERVAL; the client draws each hint as a sprite on the object until OBJECTHINT_LIFE has passed without
// a refresh. Featureful's layout (player.cpp GatherAndSendObjectHints, hud_msg.cpp MsgFunc_ObjectHint):
//
//   BYTE   OBJECTHINT_FLAG_* flags
//   SHORT  entity index (0 with OBJECTHINT_FLAG_CLOSEST: the player has no interaction hint now)
//   only with an entity index:
//   3 BYTES color (0 0 0: the HUD color)
//   COORD  scale factor
//   3 COORDS hint position
//   3 COORDS the object's size
//   STRING sprite, under OBJECTHINT_MAX_SPRITE characters
//
// At most 1 + 2 + 3 + 2 + 6 + 6 + 64 = 84 bytes.
//=========================================================

#ifndef OBJECTHINT_SHARED_H
#define OBJECTHINT_SHARED_H

#define OBJECTHINT_FLAG_CLOSEST		(1<<0)	// the interaction hint: the object +use would use now

#define OBJECTHINT_INTERVAL		0.1f	// seconds between two sends to a player (Featureful)
#define OBJECTHINT_LIFE			0.2f	// seconds a hint stays without a refresh (Featureful)
#define OBJECTHINT_MAX_SPRITE	64		// sprite path buffer, terminator included

#endif // OBJECTHINT_SHARED_H
