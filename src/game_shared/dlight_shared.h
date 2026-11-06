//=========================================================
// SBO 2.0 env_dlight: the "KeyedDLight" message, shared by the server (dlls/env_dlight.cpp) and the client
// (cl_dll/dlights.cpp). Featureful's message (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK
// license, dlls/effects.cpp CEnvDLight, cl_dll/hud_msg.cpp MsgFunc_KeyedDLight), coordinates as WRITE_COORD.
//
//   short key (the env_dlight's entity index), byte state, then by state:
//     DLIGHT_MSG_OFF   nothing: the light goes out
//     DLIGHT_MSG_ON    3 coords origin, short radius, 3 bytes color, short entity index to follow (0: none)
//     DLIGHT_MSG_POS   3 coords origin
//   at most 2 + 1 + 6 + 2 + 3 + 2 = 16 bytes
//=========================================================

#ifndef DLIGHT_SHARED_H
#define DLIGHT_SHARED_H

#define DLIGHT_MSG_OFF		0
#define DLIGHT_MSG_ON		1
#define DLIGHT_MSG_POS		2

#define DLIGHT_MSG_BYTES	16

#endif // DLIGHT_SHARED_H
