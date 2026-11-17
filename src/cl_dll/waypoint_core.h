//=========================================================
// SBO 2.1 info_waypoint, client side: the "Waypoint" message and the marker's place on the screen, without engine
// headers (tests/waypoint/run_test.sh runs them with ASan/UBSan on good, cut and hostile messages and on the edge
// placement). Format: game_shared/waypoint_shared.h; docs/INFO_WAYPOINT.md.
//=========================================================

#if !defined( WAYPOINT_CORE_H )
#define WAYPOINT_CORE_H

#include "waypoint_shared.h"

namespace waypoint
{
struct Marker
{
	int		id;					// the info_waypoint's entity index; 0: forget every marker
	int		flags;				// WAYPOINT_FLAG_*; without WAYPOINT_FLAG_ON the marker is forgotten
	float	origin[3];			// where it is (the server's last position)
	int		sprite;				// model index of the icon; 0: the default icon
	int		r, g, b;
	int		follow;				// Follow Entity index (0 none; 1 .. WAYPOINT_MAX_INDEX - 1)
	float	offset[3];			// from the Follow Entity's origin
	int		minRange, maxRange;	// units; a viewer closer / farther does not see it (0: no limit)
	int		owner;				// 0: this client's; else the player whose first-person view shows it (a spectator's)
	char	label[ WAYPOINT_MAX_LABEL ];
};

// Reads one message. False: cut short, an id or index out of range, or a position that is not a number.
bool Parse( const unsigned char *data, int size, Marker &out );

// The marker is shown at dist units from the eye (its min / max range)
bool InRange( const Marker &m, float dist );

// Where a marker is drawn on a view of w x h pixels (centre w / 2, h / 2).
//   ndcX, ndcY: the spot's projection (-1 .. 1 on the screen, y up; the triangle API's WorldToScreen), used when
//               front is true (the spot is in front of the eye)
//   right, up:  the spot's direction from the eye along the view's right and up vectors (any length), used when
//               it is behind the eye
//   marginX, marginTop, marginBottom: the room kept free at the screen's edges (the icon's half size and the arrow;
//               at the bottom also the HUD's row of numbers)
struct Place
{
	float	x, y;		// the icon's centre, pixels from the top left
	bool	edge;		// on the screen's edge: the spot is off the screen (an arrow points to it)
	float	angle;		// with edge: the direction toward the spot, radians, 0 = right, pi / 2 = down
};
Place Locate( float ndcX, float ndcY, bool front, float right, float up, float w, float h, float marginX,
	float marginTop, float marginBottom );

// Markers on the screen's edge that would cover each other (two spots in about the same direction) are moved apart
// along their edge: each one that comes within sepX across and sepY down of an earlier one goes past it (down on
// the left and right edges, right on the top and bottom ones), kept inside the screen's margins. n at most
// WAYPOINT_MAX; markers not on the edge stay where they are.
void Spread( Place *places, int n, float sepX, float sepY, float w, float h, float marginX, float marginTop,
	float marginBottom );

// "12 m": the distance in meters (units / WAYPOINT_UNITS_PER_METER, rounded down) into out (size bytes)
void FormatDistance( float units, char *out, int size );
} // namespace waypoint

#endif // WAYPOINT_CORE_H
