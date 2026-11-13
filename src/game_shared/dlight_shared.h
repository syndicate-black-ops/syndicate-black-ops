//=========================================================
// SBO 2.0 env_dlight: the "KeyedDLight" message, shared by the server (dlls/env_dlight.cpp) and the client
// (cl_dll/dlights.cpp). Featureful's message (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK
// license, dlls/effects.cpp CEnvDLight, cl_dll/hud_msg.cpp MsgFunc_KeyedDLight), coordinates as WRITE_COORD.
//
//   short key (the env_dlight's / env_elight's entity index), byte state, then by state:
//     DLIGHT_MSG_OFF   nothing: the light goes out
//     DLIGHT_MSG_ON    3 coords origin, short radius, 3 bytes color, short entity index to follow (0: none)
//     DLIGHT_MSG_POS   3 coords origin
//   SBO 2.0 env_elight: the state DLIGHT_MSG_ON | DLIGHT_MSG_ELIGHT (an entity light: models only, CL_AllocElight),
//   then the ON fields and a byte: the attachment on the followed entity (0: its origin, 1-4)
//   at most 2 + 1 + 6 + 2 + 3 + 2 + 1 = 17 bytes
//=========================================================

#ifndef DLIGHT_SHARED_H
#define DLIGHT_SHARED_H

#define DLIGHT_MSG_OFF		0
#define DLIGHT_MSG_ON		1
#define DLIGHT_MSG_POS		2
#define DLIGHT_MSG_ELIGHT	0x80	// flag on DLIGHT_MSG_ON: env_elight

#define DLIGHT_MSG_BYTES	17

#endif // DLIGHT_SHARED_H
