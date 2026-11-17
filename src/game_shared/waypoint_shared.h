//=========================================================
// SBO 2.1 info_waypoint (docs/INFO_WAYPOINT.md): the "Waypoint" user message, server dlls/info_waypoint.cpp, client
// cl_dll/waypoint.cpp (HUD marker) and cl_dll/waypoint_core.cpp (parser and screen placement). One reliable message
// per change of a marker, to every client (or with "Only the activator sees it" to its player); a client that
// (re)initializes its HUD (joining, fullupdate, a demo started mid-map) gets every marker that is on.
//
//   SHORT  id: the info_waypoint's entity index
//   BYTE   flags: WAYPOINT_FLAG_*. Without WAYPOINT_FLAG_ON the client forgets the marker (nothing follows)
//   LONG   x, y, z (float bits): where the marker is now (with a Follow Entity: that entity's origin + offset)
//   SHORT  sprite: the icon's model index (precache list); 0: the default icon (sprites/waypoint.spr)
//   BYTE   r, g, b: the color
//   SHORT  follow: the Follow Entity's index (0 none; the client puts the marker on it while it has the entity)
//   LONG   offset x, y, z (float bits): the marker's offset from the Follow Entity's origin (with follow)
//   SHORT  min range, SHORT max range: units; a viewer closer / farther does not see the marker (0: no limit)
//   BYTE   owner: 0 the marker is this client's; else (flag 1, to a spectator) the index of the player it belongs to:
//          drawn only while the client shows that player's view in first person
//   STRING label: the marker's text ("" none), at most WAYPOINT_MAX_LABEL - 1 bytes
//=========================================================

#ifndef WAYPOINT_SHARED_H
#define WAYPOINT_SHARED_H

#define WAYPOINT_MSG_NAME		"Waypoint"

#define WAYPOINT_MAX			64		// markers on at once a client keeps (more: the newest are dropped)
#define WAYPOINT_MAX_LABEL		64		// label with its terminator
#define WAYPOINT_MAX_INDEX		2048	// entity indexes (MAX_EDICTS)

#define WAYPOINT_FLAG_ON		0x01
#define WAYPOINT_FLAG_DISTANCE	0x02	// show the distance (meters) under the label
#define WAYPOINT_FLAG_FOLLOW	0x04	// follow and offset are set
#define WAYPOINT_FLAG_NO_EDGE	0x08	// no edge clamp or arrow: drawn only while its spot is in view

#define WAYPOINT_UNITS_PER_METER	39.37f	// 1 unit = 1 inch, as the Game Master HUD's distances (gmmode.cpp)

#define WAYPOINT_MSG_MAX_BYTES	( 2 + 1 + 12 + 2 + 3 + 2 + 12 + 2 + 2 + 1 + WAYPOINT_MAX_LABEL )

#endif // WAYPOINT_SHARED_H
