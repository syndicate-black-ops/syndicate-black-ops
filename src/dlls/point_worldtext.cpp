//=========================================================
// point_worldtext: text drawn in the world (Source's point_worldtext). Keyvalues and spawnflags: sdk/syndicate.fgd;
// mapper guide: docs/POINT_WORLDTEXT.md. Written for SBO on GoldSrc; the keys, their defaults and the drawing
// (orientations, spacing, rainbow) follow Source's entity (Valve Developer Community: Point_worldtext).
//
// The server keeps every text in a table (s_Texts); the clients draw them (cl_dll/worldtext.cpp) from a font atlas
// sprite (sprites/worldtext/<font>.spr). Nothing of it is a networked entity and nothing is sent per frame:
//  - Spawn puts the text into the table. A text without a name can never change (nothing can trigger it or reach
//    it with trigger_changevalue), so it then frees its edict at once, as env_static's unnamed props do: a map's
//    labels cost no entity slots. A named text keeps its edict for Use and run-time key changes, and writes every
//    change into its table entry.
//  - Each client gets the table after its HUD initialised (first InitHUD on the map, "fullupdate", demo start), a
//    few texts per tenth of a second (WorldText_SendTable, client_sendqueue.h), so a map with many long texts doesn't
//    fill the reliable channel;
//  - every client gets a text again when it is switched, removed, or a key changes at run time (trigger_changevalue,
//    trigger_copyvalue: CBaseEntity::RuntimeKeyValueChanged), at most every tenth of a second (the last state wins).
// Messages: game_shared/worldtext_shared.h; the client's id of a text is its table index. A text longer than one
// message goes in pieces first (as game_messagebox's MsgBoxPart).
//
// Use: on / off / toggle (ON / OFF as asked). Spawnflag 1 Start Off; spawnflag 2 (SBO) centers each line.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"saverestore.h"
#include	"worldtext_shared.h"
#include	"client_sendqueue.h"

#include	<string.h>

extern int gmsgWorldText;
extern int gmsgWorldTextP;

#define SF_WORLDTEXT_STARTOFF	1
#define SF_WORLDTEXT_CENTER		2	// SBO

#define WORLDTEXT_SEND_INTERVAL		0.1f	// seconds between two broadcasts of one text
#define WORLDTEXT_QUEUE_DELAY		0.5f	// after the client's InitHUD, before its first texts
#define WORLDTEXT_QUEUE_BYTES		800		// to one client per WORLDTEXT_SEND_INTERVAL (a longer text goes alone)
#define WORLDTEXT_BROADCAST_BYTES	800		// changes to everybody per WORLDTEXT_SEND_INTERVAL, all texts together
#define WORLDTEXT_MAX_ENTRIES		1024	// texts per map; more are left out (with a console message)

// One text: what the clients are sent
struct worldtext_t
{
	BOOL		used;
	BOOL		active;		// on
	string_t	message;	// as in the map (escapes resolved when sent)
	Vector		origin;
	Vector		angles;
	float		size, spacingX, spacingY;
	int			color[4];
	int			orientation;
	int			font;
	int			flags;		// WORLDTEXT_FLAG_RAINBOW, WORLDTEXT_FLAG_CENTER
};

static worldtext_t s_Texts[ WORLDTEXT_MAX_ENTRIES ];
static int s_iTexts;			// entries in use from the start of the table
static int s_iUnnamed;			// texts whose edict was freed
static BOOL s_bFullNoted;
static BOOL s_bCounted;			// the console line with the counts, once per map

// what went to everybody in this interval
static float s_flBroadcastStart = -1;
static int s_iBroadcastBytes = 0;

// ServerDeactivate: a new map starts without texts
void WorldText_Reset( void )
{
	memset( s_Texts, 0, sizeof( s_Texts ) );
	s_iTexts = 0;
	s_iUnnamed = 0;
	s_bFullNoted = FALSE;
	s_bCounted = FALSE;
	s_flBroadcastStart = -1;
	s_iBroadcastBytes = 0;
}

//---------------------------------------------------------
// Messages (of a table entry)
//---------------------------------------------------------

// The message text: game_text's convention (and game_messagebox's, game_timer's): "\n" (or "\N") starts a new line
static int BuildText( const worldtext_t &t, char *pszOut )
{
	const char *s = FStringNull( t.message ) ? "" : STRING( t.message );
	int n = 0;

	while ( *s && n < WORLDTEXT_MAX_TEXT - 1 )
	{
		if ( s[0] == '\\' && ( s[1] == 'n' || s[1] == 'N' ) )
		{
			pszOut[n++] = '\n';
			s += 2;
			continue;
		}
		if ( *s != '\r' )
			pszOut[n++] = *s;
		s++;
	}

	pszOut[n] = '\0';
	return n;
}

static int MessageSize( int i )
{
	const worldtext_t &t = s_Texts[i];
	if ( !t.active )
		return 1 + 2;

	char szText[ WORLDTEXT_MAX_TEXT ];
	int len = BuildText( t, szText );
	int size = WORLDTEXT_HEADER_BYTES + 1 + len;
	if ( len > WORLDTEXT_MAIN_TEXT_BYTES )
	{
		// the pieces: each with its short and terminator
		int parts = ( len - WORLDTEXT_MAIN_TEXT_BYTES + WORLDTEXT_PART_BYTES - 1 ) / WORLDTEXT_PART_BYTES;
		size += parts * ( 2 + 1 );
	}
	return size;
}

static int FloatBits( float f )
{
	int i;
	memcpy( &i, &f, sizeof( i ) );
	return i;
}

static int AngleShort( float a )
{
	float f = fmod( a, 360.0f );
	if ( f < 0 )
		f += 360.0f;
	return (int)( f * 65536.0f / 360.0f + 0.5f ) & 0xFFFF;
}

// The whole state of entry i, to one client
static void SendTo( int i, CBasePlayer *pPlayer )
{
	if ( !gmsgWorldText || !gmsgWorldTextP )
		return;

	const worldtext_t &t = s_Texts[i];

	if ( !t.active )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWorldText, NULL, pPlayer->pev );
			WRITE_BYTE( 0 );
			WRITE_SHORT( i );
		MESSAGE_END();
		return;
	}

	char szText[ WORLDTEXT_MAX_TEXT ];
	int len = BuildText( t, szText );
	const char *pszEnd = szText;

	// the start of a long text in pieces; the last piece (at most WORLDTEXT_MAIN_TEXT_BYTES) goes in "WorldText"
	if ( len > WORLDTEXT_MAIN_TEXT_BYTES )
	{
		int left = len;
		char szPart[ WORLDTEXT_PART_BYTES + 1 ];
		while ( left > WORLDTEXT_MAIN_TEXT_BYTES )
		{
			int n = left - WORLDTEXT_MAIN_TEXT_BYTES;
			if ( n > WORLDTEXT_PART_BYTES )
				n = WORLDTEXT_PART_BYTES;
			memcpy( szPart, pszEnd, n );
			szPart[n] = '\0';

			MESSAGE_BEGIN( MSG_ONE, gmsgWorldTextP, NULL, pPlayer->pev );
				WRITE_SHORT( i );
				WRITE_STRING( szPart );
			MESSAGE_END();

			pszEnd += n;
			left -= n;
		}
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgWorldText, NULL, pPlayer->pev );
		WRITE_BYTE( WORLDTEXT_FLAG_ON | t.flags );
		WRITE_SHORT( i );
		WRITE_LONG( FloatBits( t.origin.x ) );
		WRITE_LONG( FloatBits( t.origin.y ) );
		WRITE_LONG( FloatBits( t.origin.z ) );
		WRITE_SHORT( AngleShort( t.angles.x ) );
		WRITE_SHORT( AngleShort( t.angles.y ) );
		WRITE_SHORT( AngleShort( t.angles.z ) );
		WRITE_LONG( FloatBits( t.size ) );
		WRITE_LONG( FloatBits( t.spacingX ) );
		WRITE_LONG( FloatBits( t.spacingY ) );
		WRITE_BYTE( t.color[0] );
		WRITE_BYTE( t.color[1] );
		WRITE_BYTE( t.color[2] );
		WRITE_BYTE( t.color[3] );
		WRITE_BYTE( t.orientation );
		WRITE_BYTE( t.font );
		WRITE_STRING( pszEnd );
	MESSAGE_END();
}

// A client that gets the texts: a human player or an HLTV proxy, in the game (bots draw nothing)
static CBasePlayer *TextRecipient( int i )
{
	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

	if ( !pPlayer || FNullEnt( pPlayer->edict() ) || FStringNull( pPlayer->pev->netname ) )
		return NULL;
	if ( !( pPlayer->pev->flags & FL_CLIENT ) || ( pPlayer->pev->flags & FL_FAKECLIENT ) )
		return NULL;

	return pPlayer;
}

// Entry i to everybody now. FALSE: this interval's budget is spent (the caller sends it at the next one).
static BOOL Broadcast( int i )
{
	// a new interval?
	if ( gpGlobals->time < s_flBroadcastStart || gpGlobals->time - s_flBroadcastStart >= WORLDTEXT_SEND_INTERVAL )
	{
		s_flBroadcastStart = gpGlobals->time;
		s_iBroadcastBytes = 0;
	}

	const int iSize = MessageSize( i );
	// a text larger than the whole budget goes alone, at the start of an interval
	if ( s_iBroadcastBytes && s_iBroadcastBytes + iSize > WORLDTEXT_BROADCAST_BYTES )
		return FALSE;

	s_iBroadcastBytes += iSize;
	for ( int p = 1; p <= gpGlobals->maxClients; p++ )
	{
		CBasePlayer *pPlayer = TextRecipient( p );
		if ( pPlayer )
			SendTo( i, pPlayer );
	}
	return TRUE;
}

//---------------------------------------------------------
// The entity
//---------------------------------------------------------

class CPointWorldText : public CPointEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Think( void );
	void UpdateOnRemove( void );
	CKeyValue GetKeyValue( const char *keyName );
	void RuntimeKeyValueChanged( const char *keyName );
	int ObjectCaps( void ) { return CPointEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	void SetDefaults( void );
	void Store( void );		// the entity's state into its table entry
	void Changed( void );	// to everybody, now or in the next interval

	BOOL m_fActive;
	float m_flTextSize;
	float m_flSpacingX;
	float m_flSpacingY;
	int m_iColor[4];
	int m_iOrientation;
	int m_iFont;
	BOOL m_fRainbow;
	BOOL m_fDefaults;
	int m_iEntry;			// index in s_Texts + 1 (0: none; the private data starts zeroed)
	BOOL m_fPending;		// a change waits for the next interval (Think)
};

LINK_ENTITY_TO_CLASS( point_worldtext, CPointWorldText );

TYPEDESCRIPTION CPointWorldText::m_SaveData[] =
{
	DEFINE_FIELD( CPointWorldText, m_fActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( CPointWorldText, m_flTextSize, FIELD_FLOAT ),
	DEFINE_FIELD( CPointWorldText, m_flSpacingX, FIELD_FLOAT ),
	DEFINE_FIELD( CPointWorldText, m_flSpacingY, FIELD_FLOAT ),
	DEFINE_ARRAY( CPointWorldText, m_iColor, FIELD_INTEGER, 4 ),
	DEFINE_FIELD( CPointWorldText, m_iOrientation, FIELD_INTEGER ),
	DEFINE_FIELD( CPointWorldText, m_iFont, FIELD_INTEGER ),
	DEFINE_FIELD( CPointWorldText, m_fRainbow, FIELD_BOOLEAN ),
	DEFINE_FIELD( CPointWorldText, m_fDefaults, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CPointWorldText, CPointEntity );

// Source's "color": "r g b a", missing components 255
static void ParseColor( const char *szValue, int *pColor )
{
	int c[4] = { 255, 255, 255, 255 };
	sscanf( szValue, "%d %d %d %d", &c[0], &c[1], &c[2], &c[3] );
	for ( int i = 0; i < 4; i++ )
		pColor[i] = c[i] < 0 ? 0 : ( c[i] > 255 ? 255 : c[i] );
}

static float ClampFloat( float f, float lo, float hi )
{
	if ( !( f == f ) )	// NaN
		return lo;
	return f < lo ? lo : ( f > hi ? hi : f );
}

// Source's defaults (its constructor): size 10, white, fixed, font 0. The engine's private data starts zeroed and
// the map's keys come before Spawn, so they are set before the first key.
void CPointWorldText::SetDefaults( void )
{
	if ( m_fDefaults )
		return;
	m_fDefaults = TRUE;
	m_flTextSize = 10.0f;
	for ( int i = 0; i < 4; i++ )
		m_iColor[i] = 255;
}

void CPointWorldText::KeyValue( KeyValueData *pkvd )
{
	SetDefaults();

	// "message" is an entvars field (pev->message): DispatchKeyValue keeps it there
	if ( FStrEq( pkvd->szKeyName, "textsize" ) )
	{
		m_flTextSize = ClampFloat( atof( pkvd->szValue ), 0.0f, 4096.0f );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "textspacingX" ) )
	{
		m_flSpacingX = ClampFloat( atof( pkvd->szValue ), -4096.0f, 4096.0f );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "textspacingY" ) )
	{
		m_flSpacingY = ClampFloat( atof( pkvd->szValue ), -4096.0f, 4096.0f );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "color" ) )
	{
		ParseColor( pkvd->szValue, m_iColor );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "orientation" ) )
	{
		int i = atoi( pkvd->szValue );
		m_iOrientation = ( i >= WORLDTEXT_ORIENT_FIXED && i <= WORLDTEXT_ORIENT_UPRIGHT ) ? i : WORLDTEXT_ORIENT_FIXED;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "font" ) )
	{
		int i = atoi( pkvd->szValue );
		m_iFont = ( i >= 0 && i < WORLDTEXT_FONT_COUNT ) ? i : 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "rainbow" ) )
	{
		m_fRainbow = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CPointWorldText::Store( void )
{
	if ( !m_iEntry )
		return;

	worldtext_t &t = s_Texts[ m_iEntry - 1 ];
	t.active = m_fActive;
	t.message = pev->message;
	t.origin = pev->origin;
	t.angles = pev->angles;
	t.size = m_flTextSize;
	t.spacingX = m_flSpacingX;
	t.spacingY = m_flSpacingY;
	for ( int i = 0; i < 4; i++ )
		t.color[i] = m_iColor[i];
	t.orientation = m_iOrientation;
	t.font = m_iFont;
	t.flags = ( m_fRainbow ? WORLDTEXT_FLAG_RAINBOW : 0 ) | ( ( pev->spawnflags & SF_WORLDTEXT_CENTER ) ? WORLDTEXT_FLAG_CENTER : 0 );
}

void CPointWorldText::Spawn( void )
{
	SetDefaults();
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
	UTIL_SetOrigin( pev, pev->origin );

	m_fActive = !( pev->spawnflags & SF_WORLDTEXT_STARTOFF );

	if ( s_iTexts >= WORLDTEXT_MAX_ENTRIES )
	{
		if ( !s_bFullNoted )
			ALERT( at_console, "point_worldtext: more than %d texts, the others are left out\n", WORLDTEXT_MAX_ENTRIES );
		s_bFullNoted = TRUE;
		pev->flags |= FL_KILLME;
		return;
	}

	m_iEntry = ++s_iTexts;
	s_Texts[ m_iEntry - 1 ].used = TRUE;
	Store();

	// a text nobody can trigger or change needs no edict: the table has it (env_static's unnamed props do the same)
	if ( FStringNull( pev->targetname ) || !STRING( pev->targetname )[0] )
	{
		s_iUnnamed++;
		m_iEntry = 0;
		pev->flags |= FL_KILLME;
	}
}

void CPointWorldText::Changed( void )
{
	if ( !m_iEntry )
		return;

	Store();
	if ( Broadcast( m_iEntry - 1 ) )
	{
		m_fPending = FALSE;
		return;
	}

	m_fPending = TRUE;
	pev->nextthink = s_flBroadcastStart + WORLDTEXT_SEND_INTERVAL;
}

void CPointWorldText::Think( void )
{
	if ( m_fPending )
		Changed();
}

void CPointWorldText::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_fActive ) )
		return;

	m_fActive = !m_fActive;
	Changed();
}

// trigger_changevalue / trigger_copyvalue changed a key (keyvalue.cpp KeyValue_DispatchRuntime): "message",
// "origin" and "angles" are entvars fields the class's KeyValue never sees
void CPointWorldText::RuntimeKeyValueChanged( const char *keyName )
{
	if ( FStrEq( keyName, "origin" ) )
		UTIL_SetOrigin( pev, pev->origin );

	Changed();
}

void CPointWorldText::UpdateOnRemove( void )
{
	// killtarget: the clients forget it, and so does the table (a late joiner does not get it)
	if ( m_iEntry )
	{
		m_fActive = FALSE;
		Store();
		for ( int p = 1; p <= gpGlobals->maxClients; p++ )
		{
			CBasePlayer *pPlayer = TextRecipient( p );
			if ( pPlayer )
				SendTo( m_iEntry - 1, pPlayer );
		}
		m_iEntry = 0;
	}

	CPointEntity::UpdateOnRemove();
}

CKeyValue CPointWorldText::GetKeyValue( const char *keyName )
{
	CKeyValue keyvalue = g_emptyKeyValue;

	static const char *s_floatKeys[] = { "textsize", "textspacingX", "textspacingY" };
	static const char *s_intKeys[] = { "orientation", "font", "rainbow" };
	const float floats[] = { m_flTextSize, m_flSpacingX, m_flSpacingY };
	const int ints[] = { m_iOrientation, m_iFont, m_fRainbow ? 1 : 0 };

	for ( int i = 0; i < (int)ARRAYSIZE( s_floatKeys ); i++ )
	{
		if ( FStrEq( keyName, s_floatKeys[i] ) )
		{
			keyvalue.keyType = KEY_TYPE_FLOAT;
			keyvalue.keyName = s_floatKeys[i];
			keyvalue.fVal = floats[i];
			return keyvalue;
		}
	}

	for ( int i = 0; i < (int)ARRAYSIZE( s_intKeys ); i++ )
	{
		if ( FStrEq( keyName, s_intKeys[i] ) )
		{
			keyvalue.keyType = KEY_TYPE_INT;
			keyvalue.keyName = s_intKeys[i];
			keyvalue.iVal = ints[i];
			return keyvalue;
		}
	}

	if ( FStrEq( keyName, "color" ) )
	{
		keyvalue.keyType = KEY_TYPE_STRING;
		keyvalue.keyName = "color";
		keyvalue.sVal = ALLOC_STRING( UTIL_VarArgs( "%d %d %d %d", m_iColor[0], m_iColor[1], m_iColor[2], m_iColor[3] ) );
		return keyvalue;
	}

	// message, origin, angles, targetname, custom keyvalues, ...
	return CPointEntity::GetKeyValue( keyName );
}

//---------------------------------------------------------
// Delivery to each client after its HUD started
//---------------------------------------------------------

// The table to a client after its HUD started (client_sendqueue.h): from table entry iCursor on, while the messages
// fit in iBudget. TRUE: all sent; FALSE: go on from iCursor later.
static int WorldText_SendTable( CBasePlayer *pPlayer, int &iCursor, int &iBudget )
{
	if ( !s_bCounted && s_iTexts )
	{
		ALERT( at_console, "point_worldtext: %d texts, %d without a name (no edict)\n", s_iTexts, s_iUnnamed );
		s_bCounted = TRUE;
	}

	for ( int i = iCursor; i < s_iTexts; i++ )
	{
		if ( !s_Texts[i].used || !s_Texts[i].active )
			continue;	// the client has none after its InitHUD

		const int iSize = MessageSize( i );
		if ( iSize > iBudget && iBudget < WORLDTEXT_QUEUE_BYTES )
		{
			iCursor = i;
			return FALSE;
		}

		SendTo( i, pPlayer );
		iBudget -= iSize;
	}

	return TRUE;
}

static SendQueue_Sender s_WorldTextSender( SENDQUEUE_WORLDTEXT, WorldText_SendTable, 0, WORLDTEXT_QUEUE_DELAY,
	WORLDTEXT_SEND_INTERVAL, WORLDTEXT_QUEUE_BYTES );
