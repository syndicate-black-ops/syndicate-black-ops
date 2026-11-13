//=========================================================
// SBO 2.0 info_light_origin (docs/ENV_DLIGHT.md#info_light_origin): the "LightOrigin" user message, server
// dlls/light_origin.cpp, client cl_dll/light_origin.cpp. A model entity with a "lightorigin" key (Trinity renderer's
// key: the name of an info_light_origin) takes its lighting from the marker's position instead of from the
// lightmap under its own origin.
//
//   SHORT entity index, SHORT its model index (the client uses the point only while the entity has this model),
//   3 COORD the marker's origin
//   10 bytes (LIGHTORIGIN_MSG_BYTES: with the message header, for the send budget)
//
// env_static props carry it in their own message (static_props_shared.h STATICPROP_LIGHTORIGIN).
//=========================================================

#ifndef LIGHT_ORIGIN_SHARED_H
#define LIGHT_ORIGIN_SHARED_H

#define LIGHTORIGIN_MSG_BYTES	13

#endif // LIGHT_ORIGIN_SHARED_H
