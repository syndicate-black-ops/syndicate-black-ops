//=========================================================
// Tutor: the players' tutor windows and message queues; the TutorText / TutorClose messages. See tutor.h.
//
// CZ (ReGameDLL_CS CCSTutor) shows one message at a time; a newer message waits in an event list until the shown
// one closes, or replaces it at once if it may interrupt ("Interrupt Now") and its priority is not lower. A
// waiting message expires after its lifetime. The same rules here, per player, with the settings of the env_tutor
// that was triggered instead of tutordata.txt's definitions. Differences: the waiting message with the highest
// priority (then the oldest) is shown next (CZ: the oldest); a shown message is not cut short by a waiting one
// after a minimum display time (CZ's _tutor_message_minimum_display_time): it stays for its whole holdtime.
// Players in spectator mode (m_bSpectating) get no tutor messages.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"tutor.h"

extern int gmsgTutorText;
extern int gmsgTutorPart;
extern int gmsgTutorClose;

#define TUTOR_QUEUE_SIZE		16

// The engine's user message limit (MAX_USER_MSG_DATA 192), with some room
#define TUTOR_MAX_MSG_BYTES		188
#define TUTOR_TEXT_PART_BYTES	180

typedef struct
{
	EHANDLE		hTutor;
	float		queuedTime;
} tutorqueued_t;

typedef struct
{
	EHANDLE			hShown;				// the env_tutor in the window
	BOOL			windowOpen;
	float			closeTime;			// 0 = until triggered off
	int				shownPriority;
	BOOL			resend;				// triggered again while shown: TutorText again once the client is ready
	tutorqueued_t	queue[ TUTOR_QUEUE_SIZE ];
	int				queueCount;
} tutorplayer_t;

static tutorplayer_t s_players[ TUTOR_MAX_PLAYERS + 1 ];

static BOOL IsValidSlot( int playerIndex )
{
	return playerIndex >= 1 && playerIndex <= TUTOR_MAX_PLAYERS && playerIndex <= gpGlobals->maxClients;
}

static CBasePlayer *TutorPlayer( int playerIndex )
{
	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( playerIndex );

	if ( !pPlayer || FNullEnt( pPlayer->edict() ) || FStringNull( pPlayer->pev->netname ) )
		return NULL;

	// bots and HLTV proxies have no client to show it
	if ( pPlayer->pev->flags & ( FL_FAKECLIENT | FL_PROXY ) )
		return NULL;

	return pPlayer;
}

static void ResetPlayer( int playerIndex )
{
	memset( &s_players[ playerIndex ], 0, sizeof( s_players[ playerIndex ] ) );
}

//---------------------------------------------------------
// Messages
//---------------------------------------------------------

// TutorText (docs/TUTOR_PORT.md):
//   short id (env_tutor entity index), short type, byte flags (TUTOR_FLAG_*), byte align, short x, short y
//   (-1 = from the alignment, else 0..10000 = fraction of the screen), byte icon (TutorIcon) [string icon path if
//   TUTOR_ICON_CUSTOM], string sound ("" = none), the colours set in flags (text r g b, icon r g b,
//   background r g b a, border r g b a), string text (its end; TutorPart messages sent before carry the rest)
// TutorPart: string (the next piece of a long text)
static void SendTutorText( CBasePlayer *pPlayer, CEnvTutor *pTutor )
{
	const tutorparms_t &parms = pTutor->m_parms;

	int flags = parms.colorFlags & ( TUTOR_FLAG_TEXT_COLOR | TUTOR_FLAG_ICON_COLOR | TUTOR_FLAG_BG_COLOR | TUTOR_FLAG_BORDER_COLOR );
	if ( !pPlayer->IsAlive() || pPlayer->IsObserver() )
		flags |= TUTOR_FLAG_DEAD;

	int icon = pTutor->IconId();
	const char *iconPath = ( icon == TUTOR_ICON_CUSTOM ) ? pTutor->IconPath() : "";
	const char *sound = pTutor->SoundPath();
	const char *text = FStringNull( pTutor->pev->message ) ? "" : STRING( pTutor->pev->message );

	// the size of everything but the text
	int header = 2 + 2 + 1 + 1 + 2 + 2 + 1 + (int)strlen( sound ) + 1;
	if ( icon == TUTOR_ICON_CUSTOM )
		header += (int)strlen( iconPath ) + 1;
	if ( flags & TUTOR_FLAG_TEXT_COLOR )	header += 3;
	if ( flags & TUTOR_FLAG_ICON_COLOR )	header += 3;
	if ( flags & TUTOR_FLAG_BG_COLOR )		header += 4;
	if ( flags & TUTOR_FLAG_BORDER_COLOR )	header += 4;

	int room = TUTOR_MAX_MSG_BYTES - header - 1;
	if ( room < 0 )
		room = 0;

	// a long text: the pieces that don't fit go first (the client joins the bytes, then decodes UTF-8)
	int len = (int)strlen( text );
	char part[ TUTOR_TEXT_PART_BYTES + 1 ];
	while ( len > room )
	{
		int n = len - room;
		if ( n > TUTOR_TEXT_PART_BYTES )
			n = TUTOR_TEXT_PART_BYTES;

		memcpy( part, text, n );
		part[n] = '\0';

		MESSAGE_BEGIN( MSG_ONE, gmsgTutorPart, NULL, pPlayer->pev );
			WRITE_STRING( part );
		MESSAGE_END();

		text += n;
		len -= n;
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgTutorText, NULL, pPlayer->pev );
		WRITE_SHORT( pTutor->entindex() );
		WRITE_SHORT( parms.type );
		WRITE_BYTE( flags );
		WRITE_BYTE( parms.align );
		WRITE_SHORT( parms.x < 0 ? -1 : (int)( parms.x * 10000.0f + 0.5f ) );
		WRITE_SHORT( parms.y < 0 ? -1 : (int)( parms.y * 10000.0f + 0.5f ) );
		WRITE_BYTE( icon );
		if ( icon == TUTOR_ICON_CUSTOM )
			WRITE_STRING( iconPath );
		WRITE_STRING( sound );
		if ( flags & TUTOR_FLAG_TEXT_COLOR )
		{
			WRITE_BYTE( parms.textColor[0] ); WRITE_BYTE( parms.textColor[1] ); WRITE_BYTE( parms.textColor[2] );
		}
		if ( flags & TUTOR_FLAG_ICON_COLOR )
		{
			WRITE_BYTE( parms.iconColor[0] ); WRITE_BYTE( parms.iconColor[1] ); WRITE_BYTE( parms.iconColor[2] );
		}
		if ( flags & TUTOR_FLAG_BG_COLOR )
		{
			WRITE_BYTE( parms.bgColor[0] ); WRITE_BYTE( parms.bgColor[1] ); WRITE_BYTE( parms.bgColor[2] );
			WRITE_BYTE( parms.bgColor[3] );
		}
		if ( flags & TUTOR_FLAG_BORDER_COLOR )
		{
			WRITE_BYTE( parms.borderColor[0] ); WRITE_BYTE( parms.borderColor[1] ); WRITE_BYTE( parms.borderColor[2] );
			WRITE_BYTE( parms.borderColor[3] );
		}
		WRITE_STRING( text );
	MESSAGE_END();
}

static void SendTutorClose( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgTutorClose, NULL, pPlayer->pev );
	MESSAGE_END();
}

//---------------------------------------------------------
// The window and the queue
//---------------------------------------------------------

// Closes the shown message. fireTarget: its "closetarget" (not when the env_tutor is gone or the tutor turned off)
static void CloseShown( int playerIndex, CBasePlayer *pPlayer, BOOL sendClose, BOOL fireTarget )
{
	tutorplayer_t &state = s_players[ playerIndex ];
	if ( !state.windowOpen )
		return;

	CEnvTutor *pTutor = (CEnvTutor *)(CBaseEntity *)state.hShown;

	state.windowOpen = FALSE;
	state.resend = FALSE;
	state.hShown = NULL;
	state.closeTime = 0;
	state.shownPriority = 0;

	if ( pPlayer && sendClose )
		SendTutorClose( pPlayer );

	if ( fireTarget && pTutor && pPlayer && !FStringNull( pTutor->m_iszCloseTarget ) )
		FireTargets( STRING( pTutor->m_iszCloseTarget ), pPlayer, pTutor, USE_TOGGLE, 0 );
}

static void Display( int playerIndex, CBasePlayer *pPlayer, CEnvTutor *pTutor )
{
	tutorplayer_t &state = s_players[ playerIndex ];

	SendTutorText( pPlayer, pTutor );

	state.windowOpen = TRUE;
	state.hShown = pTutor;
	state.closeTime = pTutor->m_parms.holdtime > 0 ? gpGlobals->time + pTutor->m_parms.holdtime : 0;
	state.shownPriority = pTutor->m_parms.priority;

	pTutor->RegisterShown( playerIndex );

	ALERT( at_aiconsole, "env_tutor \"%s\" shown to %s\n", STRING( pTutor->pev->targetname ), STRING( pPlayer->pev->netname ) );

	if ( !FStringNull( pTutor->pev->target ) )
		FireTargets( STRING( pTutor->pev->target ), pPlayer, pTutor, USE_TOGGLE, 0 );
}

static void RemoveQueued( tutorplayer_t &state, int i )
{
	for ( int j = i; j < state.queueCount - 1; j++ )
		state.queue[j] = state.queue[j + 1];

	state.queueCount--;
	state.queue[ state.queueCount ].hTutor = NULL;
}

// The client must have run InitHUD (it closes the window on a new map / connection): only then messages are sent
static BOOL ClientReady( CBasePlayer *pPlayer )
{
	return pPlayer->m_fGameHUDInitialized && !pPlayer->m_fInitHUD;
}

static void PlayerThink( int playerIndex )
{
	tutorplayer_t &state = s_players[ playerIndex ];
	if ( !state.windowOpen && !state.queueCount )
		return;

	CBasePlayer *pPlayer = TutorPlayer( playerIndex );
	if ( !pPlayer )
	{
		ResetPlayer( playerIndex );
		return;
	}

	// spectator mode (observer.cpp StartSpectating): no tutor messages. The window closes without its
	// "closetarget", the waiting messages are dropped; nothing is shown again when the player leaves the mode
	if ( pPlayer->m_bSpectating )
	{
		CloseShown( playerIndex, pPlayer, ClientReady( pPlayer ), FALSE );
		ResetPlayer( playerIndex );
		return;
	}

	// SBO 2.0: only players in play get tutor messages (CEnvTutor::CanShowTo). Killed or out of play for another
	// reason: as for spectator mode, the window closes without its "closetarget" and the waiting messages are dropped
	if ( !SboScoreboard_InPlay( pPlayer ) )
	{
		CloseShown( playerIndex, pPlayer, ClientReady( pPlayer ), FALSE );
		ResetPlayer( playerIndex );
		return;
	}

	if ( !ClientReady( pPlayer ) )
		return;

	const float time = gpGlobals->time;

	if ( state.windowOpen )
	{
		// the env_tutor was removed (killtarget)
		if ( !(CBaseEntity *)state.hShown )
			CloseShown( playerIndex, pPlayer, TRUE, FALSE );
		else if ( state.closeTime > 0 && time >= state.closeTime )
			CloseShown( playerIndex, pPlayer, TRUE, TRUE );
		else if ( state.resend )
		{
			// triggered again while shown (Tutor_Show): the client plays the sound again when it arrives
			state.resend = FALSE;
			SendTutorText( pPlayer, (CEnvTutor *)(CBaseEntity *)state.hShown );
		}
	}

	// drop what expired or can't be shown any more, find the next one: highest priority, then the oldest; with the
	// window open, the next one among those that may replace it (CZ: "Interrupt Now" and at least the shown
	// message's priority), even when a higher-priority message without interrupt waits too
	int best = -1;
	int bestInterrupt = -1;
	for ( int i = 0; i < state.queueCount; )
	{
		CEnvTutor *pTutor = (CEnvTutor *)(CBaseEntity *)state.queue[i].hTutor;
		float lifetime = pTutor ? pTutor->m_parms.lifetime : 0;

		if ( !pTutor || ( lifetime > 0 && time > state.queue[i].queuedTime + lifetime ) || !pTutor->CanShowTo( pPlayer ) )
		{
			RemoveQueued( state, i );
			continue;
		}

		if ( best < 0 || pTutor->m_parms.priority > ( (CEnvTutor *)(CBaseEntity *)state.queue[best].hTutor )->m_parms.priority )
			best = i;
		if ( pTutor->m_parms.interrupt && pTutor->m_parms.priority >= state.shownPriority
			&& ( bestInterrupt < 0 || pTutor->m_parms.priority > ( (CEnvTutor *)(CBaseEntity *)state.queue[bestInterrupt].hTutor )->m_parms.priority ) )
			bestInterrupt = i;
		i++;
	}

	if ( state.windowOpen )
	{
		if ( bestInterrupt < 0 )
			return;
		best = bestInterrupt;
	}

	if ( best < 0 )
		return;

	CEnvTutor *pNext = (CEnvTutor *)(CBaseEntity *)state.queue[best].hTutor;
	RemoveQueued( state, best );

	// the new TutorText replaces the window (taken off the queue first: the close target may queue messages)
	if ( state.windowOpen )
		CloseShown( playerIndex, pPlayer, FALSE, TRUE );

	Display( playerIndex, pPlayer, pNext );
}

BOOL Tutor_Show( CBasePlayer *pPlayer, CEnvTutor *pTutor )
{
	if ( !pPlayer || !pTutor )
		return FALSE;

	int playerIndex = pPlayer->entindex();
	if ( !IsValidSlot( playerIndex ) || !TutorPlayer( playerIndex ) )
		return FALSE;

	// never in spectator mode (dead and waiting players, players in the menus: CanShowTo)
	if ( pPlayer->m_bSpectating )
		return FALSE;

	tutorplayer_t &state = s_players[ playerIndex ];

	// shown now: triggered again, it stays for another holdtime, and the TutorText is sent again so the client
	// plays its sound again (it plays it when the message arrives) and the window shows it fresh. The same show
	// goes on: no new show counted (maxshows), no "target" fired. (Playtest 2026-10-17: a message "Show on player
	// spawn" was silent at a respawn within its holdtime.)
	if ( state.windowOpen && (CBaseEntity *)state.hShown == (CBaseEntity *)pTutor )
	{
		if ( pTutor->m_parms.holdtime > 0 )
			state.closeTime = gpGlobals->time + pTutor->m_parms.holdtime;
		state.resend = TRUE;	// sent in this frame's StartFrame (PlayerThink; at a spawn the client is ready a frame later)
		return TRUE;
	}

	if ( !pTutor->CanShowTo( pPlayer ) )
		return FALSE;

	// waiting already
	for ( int i = 0; i < state.queueCount; i++ )
	{
		if ( (CBaseEntity *)state.queue[i].hTutor == (CBaseEntity *)pTutor )
			return TRUE;
	}

	// lifetime -1: only if the window is free, or it may interrupt the shown message now
	if ( pTutor->m_parms.lifetime < 0 && state.windowOpen
		&& ( !pTutor->m_parms.interrupt || pTutor->m_parms.priority < state.shownPriority ) )
		return FALSE;

	// a full queue loses its oldest message
	if ( state.queueCount >= TUTOR_QUEUE_SIZE )
		RemoveQueued( state, 0 );

	state.queue[ state.queueCount ].hTutor = pTutor;
	state.queue[ state.queueCount ].queuedTime = gpGlobals->time;
	state.queueCount++;

	// shown in this frame's StartFrame (or as soon as the client is ready)
	return TRUE;
}

void Tutor_Close( CBasePlayer *pPlayer, CEnvTutor *pTutor )
{
	if ( !pPlayer || !pTutor )
		return;

	int playerIndex = pPlayer->entindex();
	if ( !IsValidSlot( playerIndex ) )
		return;

	tutorplayer_t &state = s_players[ playerIndex ];

	for ( int i = 0; i < state.queueCount; )
	{
		if ( (CBaseEntity *)state.queue[i].hTutor == (CBaseEntity *)pTutor )
			RemoveQueued( state, i );
		else
			i++;
	}

	if ( state.windowOpen && (CBaseEntity *)state.hShown == (CBaseEntity *)pTutor )
		CloseShown( playerIndex, pPlayer, TRUE, TRUE );
}

void Tutor_StartFrame( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients && i <= TUTOR_MAX_PLAYERS; i++ )
		PlayerThink( i );
}

void Tutor_ClientPutInServer( CBasePlayer *pPlayer )
{
	int playerIndex = pPlayer->entindex();
	if ( !IsValidSlot( playerIndex ) )
		return;

	// every map: the client closed its window when it (re)connected (InitHUD)
	ResetPlayer( playerIndex );
}

void Tutor_ServerDeactivate( void )
{
	// the env_tutor entities go with the map
	for ( int i = 1; i <= TUTOR_MAX_PLAYERS; i++ )
		ResetPlayer( i );
}

void Tutor_ClientDisconnected( edict_t *pClient )
{
	int playerIndex = ENTINDEX( pClient );
	if ( !IsValidSlot( playerIndex ) )
		return;

	ResetPlayer( playerIndex );

	// the next player in this slot hasn't seen any message yet
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "env_tutor" ) ) != NULL )
		( (CEnvTutor *)pEntity )->ForgetPlayer( playerIndex );
}

void Tutor_PlayerSpawned( CBasePlayer *pPlayer )
{
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "env_tutor" ) ) != NULL )
	{
		if ( pEntity->pev->spawnflags & SF_TUTOR_ON_SPAWN )
			Tutor_Show( pPlayer, (CEnvTutor *)pEntity );
	}
}
