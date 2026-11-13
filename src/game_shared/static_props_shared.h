//=========================================================
// SBO 2.0 env_static (docs/ENV_STATIC.md): the "StaticProp" user message, server dlls/env_static.cpp, client
// cl_dll/static_props.cpp.
//
//   BYTE  STATICPROP_* type, then:
//   CLEAR    -                         the client forgets all props (first message of every send to a client)
//   MODEL    BYTE model id, STRING model file name (maps the id; sent before the first prop using it)
//   PROP     SHORT prop id, BYTE model id, 3 LONG origin (1/8 unit), 3 ANGLE angles, BYTE rendermode,
//            BYTE renderfx, 3 BYTE rendercolor, BYTE renderamt, BYTE body, BYTE skin, SHORT sequence,
//            SHORT scale (1/256), SHORT framerate (1/256), BYTE visible
//   VISIBLE  SHORT prop id, BYTE visible
//   LIGHTORIGIN  SHORT prop id, 3 LONG origin (1/8 unit): SBO 2.0 info_light_origin, the point the prop is lit from
//            (sent right after its PROP)
//=========================================================

#ifndef STATIC_PROPS_SHARED_H
#define STATIC_PROPS_SHARED_H

#define STATICPROP_CLEAR	0
#define STATICPROP_MODEL	1
#define STATICPROP_PROP		2
#define STATICPROP_VISIBLE	3
#define STATICPROP_LIGHTORIGIN	4	// SBO 2.0: info_light_origin

#define STATICPROP_MAX_PROPS	1024	// props per map
#define STATICPROP_MAX_MODELS	256		// distinct model files per map

#define STATICPROP_PROP_BYTES	36		// a PROP message, with the message header (for the send budget)
#define STATICPROP_LIGHTORIGIN_BYTES	18	// a LIGHTORIGIN message, with the message header

#endif // STATIC_PROPS_SHARED_H
