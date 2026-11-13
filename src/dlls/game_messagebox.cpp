//=========================================================
// game_messagebox: see game_messagebox.h and docs/MESSAGE_BOX.md.
//
// halflife-featureful (dlls/maprules.cpp CGameMessageBox, dlls/player.cpp, dlls/client.cpp):
//   * Use: to every player ("All Players") or to the activator. With an autoclose distance the box is not shown to a
//     player who is farther (2D) than it from the box's position (the entity, or the "position" entity), and closes
//     once he gets farther (3D) than it. Without a "message" nothing is shown.
//   * Each player has up to 8 open boxes; the same entity is not opened twice; a 9th drops the oldest.
//   * The client closes the newest box on the primary attack and tells the server ("close_messagebox <id>").
//   * The open boxes are sent again when the client's HUD is initialized.
// SBO: bots and HLTV proxies get none (no client to close them); a box is sent once the player's HUD exists (InitHUD
// clears the client's boxes, and sends them all again: a box triggered while the player was loading arrives then).
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"saverestore.h"
#include	"game_messagebox.h"

extern int gmsgMessageBox;
extern int gmsgMsgBoxPart;

// The engine's user message limit (MAX_USER_MSG_DATA 192), with some room (as tutor.cpp)
#define MSGBOX_MAX_MSG_BYTES	188
#define MSGBOX_TEXT_PART_BYTES	180

typedef struct
{
	EHANDLE		hEntity;	// the game_messagebox
	int			id;			// its entity index: the client's box id
	Vector		origin;		// autoclose: where the distance is measured from (fixed when the box was opened)
	float		distance;	// autoclose distance, 0 = none
	string_t	title;		// the entity's "title" when the box was opened (0 = none)
} msgbox_t;

typedef struct
{
	msgbox_t	boxes[ MSGBOX_MAX_BOXES ];	// oldest first
	int			count;
} msgboxplayer_t;

static msgboxplayer_t s_players[ MSGBOX_MAX_PLAYERS + 1 ];

static BOOL IsValidSlot( int playerIndex )
{
	return playerIndex >= 1 && playerIndex <= MSGBOX_MAX_PLAYERS && playerIndex <= gpGlobals->maxClients;
}

// A human player in the game (spectators and dead players too)
static CBasePlayer *BoxPlayer( int playerIndex )
{
	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( playerIndex );

	if ( !pPlayer || FNullEnt( pPlayer->edict() ) || FStringNull( pPlayer->pev->netname ) )
		return NULL;

	if ( !( pPlayer->pev->flags & FL_CLIENT ) || ( pPlayer->pev->flags & ( FL_FAKECLIENT | FL_PROXY ) ) )
		return NULL;

	return pPlayer;
}

static void ResetPlayer( int playerIndex )
{
	s_players[ playerIndex ] = msgboxplayer_t();
}

// featureful ClearMessageBoxByIndex + RemoveMessageBoxGaps
static void RemoveBox( msgboxplayer_t &state, int i )
{
	for ( int j = i + 1; j < state.count; j++ )
		state.boxes[ j - 1 ] = state.boxes[ j ];

	state.count--;
	state.boxes[ state.count ] = msgbox_t();
}

//---------------------------------------------------------
// Messages
//---------------------------------------------------------

// MessageBox (docs/MESSAGE_BOX.md): byte 1 (open), short id, string title, string text; or byte 0 (close), short id.
// MsgBoxPart: byte 0 = text / 1 = title, string (the next piece of the start of a long text or title).
// A user message carries at most 192 bytes: when title and text don't fit into one MessageBox, the whole title and
// the start of the text go first in MsgBoxPart messages, and the MessageBox carries the end of the text.
static void SendParts( CBasePlayer *pPlayer, int which, const char *&text, int &len, int keep )
{
	char part[ MSGBOX_TEXT_PART_BYTES + 1 ];
	while ( len > keep )
	{
		int n = len - keep;
		if ( n > MSGBOX_TEXT_PART_BYTES )
			n = MSGBOX_TEXT_PART_BYTES;

		memcpy( part, text, n );
		part[n] = '\0';

		MESSAGE_BEGIN( MSG_ONE, gmsgMsgBoxPart, NULL, pPlayer->pev );
			WRITE_BYTE( which );
			WRITE_STRING( part );
		MESSAGE_END();

		text += n;
		len -= n;
	}
}

static void SendOpen( CBasePlayer *pPlayer, int id, const char *title, const char *text )
{
	// what the MessageBox has room for besides the byte, the short and the two strings' terminators
	const int room = MSGBOX_MAX_MSG_BYTES - ( 1 + 2 ) - 2;

	int titleLen = (int)strlen( title );
	int textLen = (int)strlen( text );

	// the pieces that don't fit go first (the client joins the bytes, then decodes UTF-8)
	if ( titleLen + textLen > room )
	{
		SendParts( pPlayer, 1, title, titleLen, 0 );
		SendParts( pPlayer, 0, text, textLen, room );
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgMessageBox, NULL, pPlayer->pev );
		WRITE_BYTE( 1 );
		WRITE_SHORT( id );
		WRITE_STRING( title );
		WRITE_STRING( text );
	MESSAGE_END();
}

static void SendClose( CBasePlayer *pPlayer, int id )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgMessageBox, NULL, pPlayer->pev );
		WRITE_BYTE( 0 );
		WRITE_SHORT( id );
	MESSAGE_END();
}

static const char *BoxText( CBaseEntity *pEntity )
{
	if ( !pEntity || FStringNull( pEntity->pev->message ) )
		return NULL;

	const char *text = STRING( pEntity->pev->message );
	return text[0] ? text : NULL;
}

// The client has a HUD to show it: else InitHUD (MsgBox_SendAll) sends it
static BOOL ClientReady( CBasePlayer *pPlayer )
{
	return pPlayer->m_fGameHUDInitialized;
}

//---------------------------------------------------------
// game_messagebox
//---------------------------------------------------------
class CGameMessageBox : public CPointEntity
{
public:
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

private:
	void	SetForPlayer( CBasePlayer *pPlayer );
	BOOL	CalcPosition( Vector &result );

	string_t	m_position;		// "position": the entity (or "x y z") the autoclose distance is measured from
	float		m_distance;		// "distance": autoclose distance, 0 = none
	string_t	m_title;		// "title" (SBO): the box's title bar, empty = none
};

LINK_ENTITY_TO_CLASS( game_messagebox, CGameMessageBox );

TYPEDESCRIPTION CGameMessageBox::m_SaveData[] =
{
	DEFINE_FIELD( CGameMessageBox, m_position, FIELD_STRING ),
	DEFINE_FIELD( CGameMessageBox, m_distance, FIELD_FLOAT ),
	DEFINE_FIELD( CGameMessageBox, m_title, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CGameMessageBox, CPointEntity );

void CGameMessageBox::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "position" ) )
	{
		m_position = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "distance" ) )
	{
		m_distance = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "title" ) )
	{
		m_title = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CGameMessageBox::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->spawnflags & SF_MESSAGEBOX_ALLPLAYERS )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			// SBO: "All Players" means the players in play: not dead, not in the join menus, not
			// observing or spectating (the box takes the mouse until closed). The activator path below is unchanged.
			CBasePlayer *pPlayer = BoxPlayer( i );
			if ( pPlayer && SboScoreboard_InPlay( pPlayer ) )
				SetForPlayer( pPlayer );
		}
	}
	else if ( pActivator && pActivator->IsNetClient() )
	{
		CBasePlayer *pPlayer = BoxPlayer( pActivator->entindex() );
		if ( pPlayer )
			SetForPlayer( pPlayer );
	}
}

// featureful: the "position" key is a locus position ([LP]: a vector or an entity's CalcPosition, its origin).
// SBO reads it as "x y z" (that point) or a name (that entity's origin; a brush entity without an origin brush: the
// center of its bounds), not as a locus key.
BOOL CGameMessageBox::CalcPosition( Vector &result )
{
	result = pev->origin;
	if ( FStringNull( m_position ) )
		return TRUE;

	const char *pos = STRING( m_position );
	const char *p = pos;
	if ( *p == '-' || *p == '+' )
		p++;
	if ( ( *p >= '0' && *p <= '9' ) || *p == '.' )
	{
		Vector tmp;
		UTIL_StringToVector( (float *)tmp, pos );
		result = tmp;
		return TRUE;
	}

	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, pos );
	if ( !pTarget )
	{
		ALERT( at_error, "game_messagebox \"%s\" has bad or missing position \"%s\"\n", STRING( pev->targetname ), pos );
		return FALSE;
	}

	const char *model = FStringNull( pTarget->pev->model ) ? "" : STRING( pTarget->pev->model );
	if ( pTarget->pev->solid == SOLID_BSP || model[0] == '*' )
	{
		if ( pTarget->pev->origin == g_vecZero )
			result = ( pTarget->pev->absmin + pTarget->pev->absmax ) * 0.5f;
		else
			result = pTarget->pev->origin;
	}
	else
		result = pTarget->pev->origin;

	return TRUE;
}

void CGameMessageBox::SetForPlayer( CBasePlayer *pPlayer )
{
	int playerIndex = pPlayer->entindex();
	if ( !IsValidSlot( playerIndex ) )
		return;

	Vector origin = pev->origin;
	float distance = 0.0f;

	if ( m_distance > 0.0f )
	{
		distance = m_distance;

		if ( !CalcPosition( origin ) )
			return;

		if ( ( origin - pPlayer->pev->origin ).Length2D() > distance )
		{
			ALERT( at_console, "game_messagebox \"%s\": %s is too far from the message box position\n",
				STRING( pev->targetname ), STRING( pPlayer->pev->netname ) );
			return;
		}
	}

	const char *text = BoxText( this );
	if ( !text )
	{
		ALERT( at_console, "%s: message is empty\n", STRING( pev->classname ) );
		return;
	}

	// featureful AddMessageBox: not twice; a full list drops its oldest box
	msgboxplayer_t &state = s_players[ playerIndex ];
	const int id = entindex();

	for ( int i = 0; i < state.count; i++ )
	{
		if ( state.boxes[i].id == id && state.boxes[i].hEntity != NULL )
		{
			ALERT( at_console, "game_messagebox \"%s\" is open for %s already\n", STRING( pev->targetname ),
				STRING( pPlayer->pev->netname ) );
			return;
		}
	}

	if ( state.count >= MSGBOX_MAX_BOXES )
	{
		ALERT( at_console, "Too many message boxes for %s at once. Removing the oldest one\n",
			STRING( pPlayer->pev->netname ) );
		if ( ClientReady( pPlayer ) )
			SendClose( pPlayer, state.boxes[0].id );
		RemoveBox( state, 0 );
	}

	msgbox_t &box = state.boxes[ state.count++ ];
	box.hEntity = this;
	box.id = id;
	box.origin = origin;
	box.distance = distance;
	box.title = m_title;

	if ( ClientReady( pPlayer ) )
		SendOpen( pPlayer, id, FStringNull( m_title ) ? "" : STRING( m_title ), text );
}

//---------------------------------------------------------
// The players' boxes
//---------------------------------------------------------
void MsgBox_SendAll( CBasePlayer *pPlayer )
{
	int playerIndex = pPlayer->entindex();
	if ( !IsValidSlot( playerIndex ) || !BoxPlayer( playerIndex ) )
		return;

	msgboxplayer_t &state = s_players[ playerIndex ];
	for ( int i = 0; i < state.count; )
	{
		const char *text = BoxText( state.boxes[i].hEntity );
		if ( !text )
		{
			// the entity was removed (killtarget) or its text was cleared: nothing to show
			RemoveBox( state, i );
			continue;
		}

		SendOpen( pPlayer, state.boxes[i].id, FStringNull( state.boxes[i].title ) ? "" : STRING( state.boxes[i].title ), text );
		i++;
	}
}

BOOL MsgBox_ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if ( !FStrEq( pcmd, "close_messagebox" ) )
		return FALSE;

	int playerIndex = pPlayer->entindex();
	if ( CMD_ARGC() > 1 && IsValidSlot( playerIndex ) )
	{
		msgboxplayer_t &state = s_players[ playerIndex ];
		const int id = atoi( CMD_ARGV(1) );

		for ( int i = 0; i < state.count; i++ )
		{
			if ( state.boxes[i].id == id )
			{
				RemoveBox( state, i );
				break;
			}
		}
	}

	return TRUE;
}

// featureful CBasePlayer::PostThink: a box with an autoclose distance closes when the player is farther (3D)
void MsgBox_StartFrame( void )
{
	for ( int playerIndex = 1; playerIndex <= MSGBOX_MAX_PLAYERS && playerIndex <= gpGlobals->maxClients; playerIndex++ )
	{
		msgboxplayer_t &state = s_players[ playerIndex ];
		if ( !state.count )
			continue;

		CBasePlayer *pPlayer = BoxPlayer( playerIndex );
		if ( !pPlayer )
		{
			ResetPlayer( playerIndex );
			continue;
		}

		for ( int i = 0; i < state.count; )
		{
			msgbox_t &box = state.boxes[i];
			if ( box.distance > 0.0f && ( pPlayer->pev->origin - box.origin ).Length() > box.distance )
			{
				const int id = box.id;
				RemoveBox( state, i );
				if ( ClientReady( pPlayer ) )
					SendClose( pPlayer, id );
				continue;
			}
			i++;
		}
	}
}

// SBO: no weapon change while a box is open: the client keeps the slot keys and the HUD weapon
// menu shut, this refuses the weapon commands that go to the server directly (a bound "weapon_<name>", "lastinv",
// "use <weapon>")
BOOL MsgBox_BlocksWeaponCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if ( !FStrEq( pcmd, "lastinv" ) && !FStrEq( pcmd, "use" ) && strncmp( pcmd, "weapon_", 7 ) )
		return FALSE;

	int playerIndex = pPlayer->entindex();
	return IsValidSlot( playerIndex ) && s_players[ playerIndex ].count > 0;
}

void MsgBox_ClientPutInServer( CBasePlayer *pPlayer )
{
	int playerIndex = pPlayer->entindex();
	if ( IsValidSlot( playerIndex ) )
		ResetPlayer( playerIndex );	// a new connection or a new map: the client has no box
}

void MsgBox_ClientDisconnected( edict_t *pClient )
{
	int playerIndex = ENTINDEX( pClient );
	if ( IsValidSlot( playerIndex ) )
		ResetPlayer( playerIndex );
}

void MsgBox_ServerDeactivate( void )
{
	for ( int i = 1; i <= MSGBOX_MAX_PLAYERS; i++ )
		ResetPlayer( i );
}
