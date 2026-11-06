//=========================================================
// SBO 2.0: per-client queued senders -- map state a client gets after its HUD was (re)initialised, a few messages at
// a time, so a map with hundreds of entries never fills a client's reliable channel (from Aurora's env_particle
// sender). Users: env_particle (Aurora systems), env_static (props), point_worldtext (texts), ambient_music /
// ambient_generic (sound.cpp: the map's music and play mode sounds for a client that joined), vgui_screen (world
// screens and their kept control state), env_dlight (the dynamic lights that are on), head_scale.cpp (players' head scales),
// killfeed_icons.cpp (the killfeed icons plugins set).
//
// A sender registers once (a static SendQueue_Sender object in its .cpp) under its id. CBasePlayer starts all
// senders at InitHUD / a HUD resend (SendQueue_StartAll) and runs them from UpdateClientData (SendQueue_Run).
// Bots get nothing.
//=========================================================

#ifndef CLIENT_SENDQUEUE_H
#define CLIENT_SENDQUEUE_H

class CBasePlayer;

enum
{
	SENDQUEUE_AURORA = 0,	// env_particle.cpp
	SENDQUEUE_STATIC,		// env_static.cpp
	SENDQUEUE_WORLDTEXT,	// point_worldtext.cpp
	SENDQUEUE_MUSIC,		// sound.cpp (ambient_music, ambient_generic play modes)
	SENDQUEUE_WORLDSCREEN,	// vgui_screen.cpp
	SENDQUEUE_DLIGHT,		// env_dlight.cpp
	SENDQUEUE_HEADSCALE,	// head_scale.cpp (players' head scales)
	SENDQUEUE_KFICON,		// killfeed_icons.cpp (plugins' killfeed icons)
	SENDQUEUE_MAX = 8		// full: a new sender needs a larger SENDQUEUE_MAX
};

// Sends the next messages to pPlayer from iCursor on while they fit in iBudget (bytes; take off what is sent). TRUE:
// all sent. FALSE: the rest later; iCursor says where to go on.
typedef int (*SENDQUEUE_FN)( CBasePlayer *pPlayer, int &iCursor, int &iBudget );

// one sender's state in a player (CBasePlayer::m_SendQueue)
struct sendqueue_state_t
{
	int		sending;	// BOOL
	int		cursor;
	float	nextSend;	// nothing before this time
};

// registers a sender (a static object in the sender's .cpp)
class SendQueue_Sender
{
public:
	// iStart: the first cursor; flDelay: after the start, before the first messages; flInterval: between two
	// rounds; iBytes: the budget of a round
	SendQueue_Sender( int id, SENDQUEUE_FN pfn, int iStart, float flDelay, float flInterval, int iBytes );
};

void SendQueue_Start( CBasePlayer *pPlayer, int id );
void SendQueue_StartAll( CBasePlayer *pPlayer );	// the client's HUD was (re)initialised
void SendQueue_Run( CBasePlayer *pPlayer );			// UpdateClientData, every frame

#endif // CLIENT_SENDQUEUE_H
