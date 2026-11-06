//=========================================================
// vgui_screen and vgui_control_proxy, see vgui_screen.h and docs/WORLD_SCREENS.md.
//
// SBO Source's vgui_screen, mapped to GoldSrc (no entity inputs and outputs, no networked variables for a point
// entity):
//   * SetActive / SetInactive: Use (ON, OFF, TOGGLE); spawnflag 1 "Start inactive".
//   * OnCommand: "target", fired on every button command; "~<command>": the target fired for that command.
//     Activator: the player who clicked.
//   * SetControlText / Enabled / Visible: vgui_control_proxy (keys "screen", "controlname", "mode", "text"), fired.
//   * The screen's data: the WScreen message, to everybody on a change and to every player who enters the game.
//     Unlike Source, the latest state of each control the map changed (text / value, enabled, visible) and the
//     latest animation sequence per state group are kept and sent to players who join later, through the shared
//     per-client send queue (client_sendqueue.h: a byte budget per player and round, so a map with many screens
//     can't overflow a joining client's reliable channel).
//   * Spawnflag 2 "Input Disabled": a display only screen; its commands are refused.
//   * The server checks a command: an active screen, a living player who is not spectating, standing in front of
//     the screen within "inputdistance" of its plane and its rectangle, not more than 10 commands a second.
//   * SBO 2.0 addition, "surface" (bound mode): the panel is shown on the faces of a brush entity (the carrier)
//     that have the tool texture {screen, stretched as the texture is aligned on them. The client reads those faces
//     from the map's .bsp itself; the server sends the carrier's entity index and submodel, sets the carrier's
//     render mode to Solid (amount 255) so that the {screen faces are invisible, and checks a command against the
//     carrier's box (absmin / absmax, which follow a moving carrier).
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"saverestore.h"
#include	"vgui_screen.h"
#include	"video_shared.h"	// game_shared: movie names
#include	"vgui_screen_shared.h"
#include	"client_sendqueue.h"

extern int gmsgWScreen;
extern int CountPlayers( void );

// The client's reach test, plus this much for movement and latency between the click and the command
#define VGUI_SCREEN_REACH_SLACK		32.0f
#define VGUI_SCREEN_COMMAND_INTERVAL	0.1f

// The send queue (client_sendqueue.h), as point_worldtext's (names and strings of the same sizes): after the client's
// InitHUD this long before its first screens, then at most this many bytes of WScreen messages per round over all
// screens (a longer message goes alone), a round every this long
#define VGUI_SCREEN_QUEUE_DELAY		0.5f
#define VGUI_SCREEN_QUEUE_BYTES		800
#define VGUI_SCREEN_QUEUE_INTERVAL	0.1f
// the queue's cursor: entity index * this + the screen's next kept item (KeptItems)
#define VGUI_SCREEN_QUEUE_STRIDE	256
#if 1 + 3 * VGUI_SCREEN_MAX_CONTROLS + VGUI_SCREEN_MAX_ANIM_GROUPS > VGUI_SCREEN_QUEUE_STRIDE
#error VGUI_SCREEN_QUEUE_STRIDE: a screen has more kept items
#endif
#define VGUI_SCREEN_MSG_OVERHEAD	3	// the user message's id and length

static float s_flNextCommand[ 33 ];	// per player index: the time the next command is accepted

LINK_ENTITY_TO_CLASS( vgui_screen, CVGuiScreen );

TYPEDESCRIPTION	CVGuiScreen::m_SaveData[] =
{
	DEFINE_FIELD( CVGuiScreen, m_iszPanelName, FIELD_STRING ),
	DEFINE_FIELD( CVGuiScreen, m_iszMovie, FIELD_STRING ),
	DEFINE_FIELD( CVGuiScreen, m_bLooping, FIELD_BOOLEAN ),
	DEFINE_FIELD( CVGuiScreen, m_flRadius, FIELD_FLOAT ),
	DEFINE_FIELD( CVGuiScreen, m_flMovieStart, FIELD_TIME ),
	DEFINE_FIELD( CVGuiScreen, m_flWidth, FIELD_FLOAT ),
	DEFINE_FIELD( CVGuiScreen, m_flHeight, FIELD_FLOAT ),
	DEFINE_FIELD( CVGuiScreen, m_flInputDistance, FIELD_FLOAT ),
	DEFINE_FIELD( CVGuiScreen, m_bNoCursor, FIELD_BOOLEAN ),
	DEFINE_FIELD( CVGuiScreen, m_bActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( CVGuiScreen, m_iszSurface, FIELD_STRING ),
	DEFINE_FIELD( CVGuiScreen, m_bBound, FIELD_BOOLEAN ),
	DEFINE_FIELD( CVGuiScreen, m_hCarrier, FIELD_EHANDLE ),
	DEFINE_FIELD( CVGuiScreen, m_iCommandTargets, FIELD_INTEGER ),
	DEFINE_ARRAY( CVGuiScreen, m_iszCommands, FIELD_STRING, VGUI_SCREEN_MAX_COMMAND_TARGETS ),
	DEFINE_ARRAY( CVGuiScreen, m_iszCommandTargets, FIELD_STRING, VGUI_SCREEN_MAX_COMMAND_TARGETS ),
	DEFINE_FIELD( CVGuiScreen, m_iControls, FIELD_INTEGER ),
	DEFINE_ARRAY( CVGuiScreen, m_iszControlNames, FIELD_STRING, VGUI_SCREEN_MAX_CONTROLS ),
	DEFINE_ARRAY( CVGuiScreen, m_iszControlTexts, FIELD_STRING, VGUI_SCREEN_MAX_CONTROLS ),
	DEFINE_ARRAY( CVGuiScreen, m_iControlFlags, FIELD_INTEGER, VGUI_SCREEN_MAX_CONTROLS ),
	DEFINE_FIELD( CVGuiScreen, m_iAnims, FIELD_INTEGER ),
	DEFINE_ARRAY( CVGuiScreen, m_iszAnimGroups, FIELD_STRING, VGUI_SCREEN_MAX_ANIM_GROUPS ),
	DEFINE_ARRAY( CVGuiScreen, m_iszAnimSequences, FIELD_STRING, VGUI_SCREEN_MAX_ANIM_GROUPS ),
};

IMPLEMENT_SAVERESTORE( CVGuiScreen, CPointEntity );

void CVGuiScreen::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "panelname" ) )
	{
		m_iszPanelName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "moviefilename" ) )
	{
		if ( pkvd->szValue[0] )
			m_iszMovie = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "looping" ) )
	{
		m_bLooping = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "radius" ) )
	{
		m_flRadius = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "width" ) )
	{
		m_flWidth = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "height" ) )
	{
		m_flHeight = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "inputdistance" ) )
	{
		m_flInputDistance = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "surface" ) )
	{
		if ( pkvd->szValue[0] )
			m_iszSurface = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "showcursor" ) )
	{
		m_bNoCursor = ( atoi( pkvd->szValue ) == 0 );
		pkvd->fHandled = TRUE;
	}
	else if ( pkvd->szKeyName[0] == '~' )
	{
		// Source's per-command output: "~floor1" "elevator_floor1"
		const char *command = pkvd->szKeyName + 1;
		if ( !WScreen_ValidName( command, WSCREEN_MAX_COMMAND, 1 ) || !pkvd->szValue[0] )
			ALERT( at_console, "vgui_screen: bad key \"%s\" \"%s\" (a button command and a target name)\n",
				pkvd->szKeyName, pkvd->szValue );
		else if ( m_iCommandTargets >= VGUI_SCREEN_MAX_COMMAND_TARGETS )
			ALERT( at_console, "vgui_screen: more than %d \"~<command>\" keys, \"%s\" ignored\n",
				VGUI_SCREEN_MAX_COMMAND_TARGETS, pkvd->szKeyName );
		else
		{
			m_iszCommands[ m_iCommandTargets ] = ALLOC_STRING( command );
			m_iszCommandTargets[ m_iCommandTargets ] = ALLOC_STRING( pkvd->szValue );
			m_iCommandTargets++;
		}
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CVGuiScreen::Spawn( void )
{
	if ( IsMovie() && !VideoShared_IsValidName( STRING( m_iszMovie ), true ) )
	{
		// printed at every developer level: the mapper must see it
		g_engfuncs.pfnServerPrint( UTIL_VarArgs( "Error: vgui_screen at (%.0f %.0f %.0f): \"moviefilename\" \"%s\" refused (%s): "
			"removed\n", pev->origin.x, pev->origin.y, pev->origin.z, STRING( m_iszMovie ),
			VideoShared_NameError( STRING( m_iszMovie ), true ) ) );
		REMOVE_ENTITY( edict() );
		return;
	}
	// a movie screen shows no panel: "panelname" is not needed (the FGD's default is ignored)
	if ( IsMovie() )
		m_iszPanelName = MAKE_STRING( "movie" );
	if ( m_flRadius <= 0 )
		m_flRadius = VGUI_SCREEN_DEFAULT_MOVIE_RADIUS;

	if ( FStringNull( m_iszPanelName ) || !WScreen_ValidName( STRING( m_iszPanelName ), WSCREEN_MAX_PANELNAME, 0 ) )
	{
		ALERT( at_error, "vgui_screen at (%.0f %.0f %.0f): \"panelname\" missing or not a plain name (letters, digits, "
			"'_', '-', at most %d): removed\n", pev->origin.x, pev->origin.y, pev->origin.z, WSCREEN_MAX_PANELNAME - 1 );
		REMOVE_ENTITY( edict() );
		return;
	}

	// the FGD's defaults for keys the map leaves out (or sets to 0)
	if ( m_flWidth <= 0 )
		m_flWidth = 32;
	if ( m_flHeight <= 0 )
		m_flHeight = 32;
	if ( m_flInputDistance <= 0 )
		m_flInputDistance = WSCREEN_DEFAULT_INPUT_DISTANCE;
	if ( m_flInputDistance > WSCREEN_MAX_INPUT_DISTANCE )
		m_flInputDistance = WSCREEN_MAX_INPUT_DISTANCE;

	m_bActive = !( pev->spawnflags & SF_VGUI_SCREEN_START_INACTIVE );
	m_flMovieStart = gpGlobals->time;

	Precache();
	CPointEntity::Spawn();
}

void CVGuiScreen::Precache( void )
{
	if ( IsMovie() )
	{
		// the movie, so players download it (FastDL); no layout
		char movie[ VIDEO_MAX_PATH ];
		if ( VideoShared_MakePath( STRING( m_iszMovie ), true, movie, sizeof( movie ) ) )
			PRECACHE_GENERIC( (char *)STRING( ALLOC_STRING( movie ) ) );
		return;
	}

	// The layout: precached when the server has it, so players download a map's own layout like its other files
	// (the mod's layouts are on every client already)
	char path[ 128 ];
	snprintf( path, sizeof( path ), "ui/scripts/screens/%s.res", STRING( m_iszPanelName ) );
	int length = 0;
	byte *data = LOAD_FILE_FOR_ME( path, &length );
	if ( data )
	{
		FREE_FILE( data );
		PRECACHE_GENERIC( (char *)STRING( ALLOC_STRING( path ) ) );
	}
	else
		ALERT( at_console, "vgui_screen \"%s\": the server has no %s (clients need it: the mod's or the map's)\n",
			STRING( pev->targetname ), path );

	// its animation script, if it has one (optional)
	snprintf( path, sizeof( path ), "ui/scripts/screens/%s_anims.txt", STRING( m_iszPanelName ) );
	data = LOAD_FILE_FOR_ME( path, &length );
	if ( data )
	{
		FREE_FILE( data );
		PRECACHE_GENERIC( (char *)STRING( ALLOC_STRING( path ) ) );
	}
}

// Bound mode: find the carrier (every entity has spawned by now) and make its {screen faces invisible.
void CVGuiScreen::Activate( void )
{
	if ( FStringNull( m_iszSurface ) || m_bBound )
		return;

	const char *name = STRING( m_iszSurface );
	CBaseEntity *pCarrier = UTIL_FindEntityByTargetname( NULL, name );
	const char *model = pCarrier ? STRING( pCarrier->pev->model ) : "";
	if ( !pCarrier || model[0] != '*' || atoi( model + 1 ) <= 0 )
	{
		ALERT( at_error, "vgui_screen \"%s\": \"surface\" \"%s\" is %s: shown on its rectangle (origin, angles, "
			"width, height) instead\n", STRING( pev->targetname ), name,
			pCarrier ? "not a brush entity (func_wall, func_door, ...)" : "no entity's name" );
		return;
	}
	if ( UTIL_FindEntityByTargetname( pCarrier, name ) )
		ALERT( at_console, "vgui_screen \"%s\": several entities are named \"%s\", the screen is on the first one "
			"(%s)\n", STRING( pev->targetname ), name, STRING( pCarrier->pev->classname ) );
	if ( pCarrier->pev->effects & EF_NODRAW )
		ALERT( at_console, "vgui_screen \"%s\": its carrier \"%s\" is not drawn (EF_NODRAW), so neither is the "
			"screen\n", STRING( pev->targetname ), name );

	// A {texture's palette index 255 is see-through only in render mode Solid (kRenderTransAlpha) with FX amount 255;
	// Solid looks like Normal on the carrier's other faces. Another mode the mapper chose is kept.
	if ( pCarrier->pev->rendermode == kRenderNormal )
	{
		pCarrier->pev->rendermode = kRenderTransAlpha;
		pCarrier->pev->renderamt = 255;
	}
	else if ( pCarrier->pev->rendermode != kRenderTransAlpha || pCarrier->pev->renderamt != 255 )
		ALERT( at_console, "vgui_screen \"%s\": its carrier \"%s\" has render mode %d, FX amount %.0f: its {screen "
			"faces are only invisible with Normal or Solid / 255\n", STRING( pev->targetname ), name,
			pCarrier->pev->rendermode, pCarrier->pev->renderamt );

	m_bBound = TRUE;
	m_hCarrier = pCarrier;
}

void CVGuiScreen::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	BOOL active = m_bActive;
	if ( useType == USE_ON )
		active = TRUE;
	else if ( useType == USE_OFF )
		active = FALSE;
	else if ( useType == USE_TOGGLE )
		active = !m_bActive;

	if ( active == m_bActive )
		return;
	m_bActive = active;
	if ( active )
		m_flMovieStart = gpGlobals->time;	// a movie starts over (the clients restart it with this STATE)
	SendState();
}

static int ScreenFlags( BOOL active, BOOL noCursor, BOOL bound, BOOL noInput )
{
	return ( active ? WSCREEN_FLAG_ACTIVE : 0 ) | ( noCursor ? WSCREEN_FLAG_NOCURSOR : 0 ) |
		( bound ? WSCREEN_FLAG_BOUND : 0 ) | ( noInput ? WSCREEN_FLAG_NOINPUT : 0 );
}

// a movie screen takes no input
static int MovieFlags( BOOL movie, BOOL looping, int spawnflags )
{
	if ( !movie )
		return 0;
	return WSCREEN_FLAG_MOVIE | WSCREEN_FLAG_NOINPUT | ( looping ? WSCREEN_FLAG_LOOP : 0 ) |
		( ( spawnflags & SF_VGUI_SCREEN_MOVIE_SOUND2D ) ? WSCREEN_FLAG_SOUND2D : 0 ) |
		( ( spawnflags & SF_VGUI_SCREEN_MOVIE_NOSOUND ) ? WSCREEN_FLAG_NOSOUND : 0 );
}

#define SCREEN_FLAGS	( ScreenFlags( m_bActive, m_bNoCursor, m_bBound, ( pev->spawnflags & SF_VGUI_SCREEN_INPUT_DISABLED ) != 0 ) \
						| MovieFlags( IsMovie(), m_bLooping, pev->spawnflags ) )

// "health" as ambient_generic's volume, 0 - 10; empty (0): 10
int CVGuiScreen::MovieVolume( void )
{
	int v = (int)pev->health;
	if ( v <= 0 || v > 10 )
		v = 10;
	return v;
}

static void WriteAngle16( float a )
{
	WRITE_SHORT( (int)( a * ( 65536.0f / 360.0f ) ) & 0xFFFF );
}

int CVGuiScreen::DefineSize( void )
{
	return VGUI_SCREEN_MSG_OVERHEAD + 24 + (int)strlen( STRING( m_iszPanelName ) ) + 1 + ( m_bBound ? 4 : 0 ) +
		( IsMovie() ? (int)strlen( STRING( m_iszMovie ) ) + 1 + 4 + 1 + 2 : 0 );
}

int CVGuiScreen::SendDefine( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgWScreen, NULL, pPlayer->pev );
		WRITE_BYTE( WSCREEN_OP_DEFINE );
		WRITE_SHORT( entindex() );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WriteAngle16( pev->angles.x );
		WriteAngle16( pev->angles.y );
		WriteAngle16( pev->angles.z );
		WRITE_COORD( m_flWidth );
		WRITE_COORD( m_flHeight );
		WRITE_SHORT( (int)m_flInputDistance );
		WRITE_BYTE( SCREEN_FLAGS );
		WRITE_STRING( STRING( m_iszPanelName ) );
		if ( m_bBound )
		{
			// the carrier's entity index and submodel ("*N"); 0 0 once it is removed: not shown
			CBaseEntity *pCarrier = Carrier();
			WRITE_SHORT( pCarrier ? pCarrier->entindex() : 0 );
			WRITE_SHORT( pCarrier ? atoi( STRING( pCarrier->pev->model ) + 1 ) : 0 );
		}
		if ( IsMovie() )
		{
			// the clock: milliseconds since it started, so a player who joins later sees the same frame
			float elapsed = gpGlobals->time - m_flMovieStart;
			if ( elapsed < 0 )
				elapsed = 0;
			WRITE_STRING( STRING( m_iszMovie ) );
			WRITE_LONG( (int)( elapsed * 1000.0f ) );
			WRITE_BYTE( MovieVolume() );
			WRITE_SHORT( (int)( m_flRadius > 32767 ? 32767 : m_flRadius ) );
		}
	MESSAGE_END();
	return DefineSize();
}

void CVGuiScreen::SendState( void )
{
	if ( CountPlayers() == 0 )
		return;	// the players who come get it with the DEFINE
	MESSAGE_BEGIN( MSG_ALL, gmsgWScreen );
		WRITE_BYTE( WSCREEN_OP_STATE );
		WRITE_SHORT( entindex() );
		WRITE_BYTE( SCREEN_FLAGS );
	MESSAGE_END();
}

static int ControlSize( const char *name, const char *value )
{
	return VGUI_SCREEN_MSG_OVERHEAD + 4 + (int)strlen( name ) + 1 + (int)strlen( value ) + 1;
}

int CVGuiScreen::SendControl( CBasePlayer *pPlayer, string_t control, int op, const char *value )
{
	const char *name = FStringNull( control ) ? "" : STRING( control );
	if ( pPlayer )
		MESSAGE_BEGIN( MSG_ONE, gmsgWScreen, NULL, pPlayer->pev );
	else
	{
		// no broadcast before a client is in (the engine refuses user messages until a client has their list)
		if ( CountPlayers() == 0 )
			return 0;
		MESSAGE_BEGIN( MSG_ALL, gmsgWScreen );
	}
		WRITE_BYTE( WSCREEN_OP_CONTROL );
		WRITE_SHORT( entindex() );
		WRITE_BYTE( op );
		WRITE_STRING( name );
		WRITE_STRING( value );
	MESSAGE_END();
	return ControlSize( name, value );
}

void CVGuiScreen::SetControl( string_t control, int op, string_t value )
{
	const char *text = FStringNull( value ) ? "" : STRING( value );
	SendControl( NULL, control, op, text );

	// kept for players who join later: the latest state
	if ( op == WSCREEN_CONTROL_ANIMATION )
	{
		if ( FStringNull( control ) || !STRING( control )[0] )
			return;	// no state group: a one-shot animation, not kept
		int i;
		for ( i = 0; i < m_iAnims; i++ )
		{
			if ( !stricmp( STRING( m_iszAnimGroups[i] ), STRING( control ) ) )
				break;
		}
		if ( i == m_iAnims )
		{
			if ( m_iAnims >= VGUI_SCREEN_MAX_ANIM_GROUPS )
			{
				ALERT( at_console, "vgui_screen \"%s\": more than %d animation state groups, \"%s\" not kept for "
					"players who join later\n", STRING( pev->targetname ), VGUI_SCREEN_MAX_ANIM_GROUPS, STRING( control ) );
				return;
			}
			m_iAnims++;
		}
		m_iszAnimGroups[i] = control;
		m_iszAnimSequences[i] = value;
		return;
	}

	int i;
	for ( i = 0; i < m_iControls; i++ )
	{
		if ( !stricmp( STRING( m_iszControlNames[i] ), STRING( control ) ) )
			break;
	}
	if ( i == m_iControls )
	{
		if ( m_iControls >= VGUI_SCREEN_MAX_CONTROLS )
		{
			ALERT( at_console, "vgui_screen \"%s\": more than %d controls changed, \"%s\" not kept for players who "
				"join later\n", STRING( pev->targetname ), VGUI_SCREEN_MAX_CONTROLS, STRING( control ) );
			return;
		}
		m_iszControlNames[i] = control;
		m_iszControlTexts[i] = 0;
		m_iControlFlags[i] = 0;
		m_iControls++;
	}
	const bool on = text[0] == '1';
	switch ( op )
	{
	case WSCREEN_CONTROL_TEXT:
		m_iszControlTexts[i] = value;
		m_iControlFlags[i] |= VGUI_CONTROL_HAS_TEXT;
		break;
	case WSCREEN_CONTROL_ENABLED:
		m_iControlFlags[i] = ( m_iControlFlags[i] & ~VGUI_CONTROL_ENABLED ) | VGUI_CONTROL_HAS_ENABLED |
			( on ? VGUI_CONTROL_ENABLED : 0 );
		break;
	case WSCREEN_CONTROL_VISIBLE:
		m_iControlFlags[i] = ( m_iControlFlags[i] & ~VGUI_CONTROL_VISIBLE ) | VGUI_CONTROL_HAS_VISIBLE |
			( on ? VGUI_CONTROL_VISIBLE : 0 );
		break;
	}
}

// Item 0: the DEFINE; then three per kept control (text, enabled, visible), then the animation groups in the order
// they were first used (each replayed from its start: a sequence that loops goes on looping).
BOOL CVGuiScreen::KeptControl( int item, string_t &control, int &op, const char *&value )
{
	item--;
	if ( item < 0 )
		return FALSE;
	if ( item < 3 * m_iControls )
	{
		const int c = item / 3, flags = m_iControlFlags[c];
		control = m_iszControlNames[c];
		switch ( item % 3 )
		{
		case 0:
			op = WSCREEN_CONTROL_TEXT;
			value = FStringNull( m_iszControlTexts[c] ) ? "" : STRING( m_iszControlTexts[c] );
			return ( flags & VGUI_CONTROL_HAS_TEXT ) != 0;
		case 1:
			op = WSCREEN_CONTROL_ENABLED;
			value = ( flags & VGUI_CONTROL_ENABLED ) ? "1" : "0";
			return ( flags & VGUI_CONTROL_HAS_ENABLED ) != 0;
		default:
			op = WSCREEN_CONTROL_VISIBLE;
			value = ( flags & VGUI_CONTROL_VISIBLE ) ? "1" : "0";
			return ( flags & VGUI_CONTROL_HAS_VISIBLE ) != 0;
		}
	}
	item -= 3 * m_iControls;
	if ( item >= m_iAnims )
		return FALSE;
	control = m_iszAnimGroups[item];
	op = WSCREEN_CONTROL_ANIMATION;
	value = FStringNull( m_iszAnimSequences[item] ) ? "" : STRING( m_iszAnimSequences[item] );
	return TRUE;
}

int CVGuiScreen::SendKept( CBasePlayer *pPlayer, int item )
{
	if ( item == 0 )
		return pPlayer ? SendDefine( pPlayer ) : DefineSize();

	string_t control = 0;
	int op = 0;
	const char *value = "";
	if ( !KeptControl( item, control, op, value ) )
		return 0;
	if ( pPlayer )
		return SendControl( pPlayer, control, op, value );
	return ControlSize( FStringNull( control ) ? "" : STRING( control ), value );
}

// The send queue (client_sendqueue.h, SENDQUEUE_WORLDSCREEN): the screens in entity order, each one's DEFINE and
// kept items, from iCursor (entity index * VGUI_SCREEN_QUEUE_STRIDE + item) on while they fit in iBudget. Read from
// the screens when sent, so a change broadcast meanwhile is never undone. TRUE: all sent; FALSE: go on from iCursor
// later.
int CVGuiScreen::SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget )
{
	const int iFirst = iCursor / VGUI_SCREEN_QUEUE_STRIDE;
	for ( int index = max( iFirst, gpGlobals->maxClients + 1 ); index < gpGlobals->maxEntities; index++ )
	{
		edict_t *pEdict = INDEXENT( index );
		if ( FNullEnt( pEdict ) || pEdict->free || !FClassnameIs( pEdict, "vgui_screen" ) )
			continue;
		CVGuiScreen *pScreen = (CVGuiScreen *)CBaseEntity::Instance( pEdict );
		if ( !pScreen )
			continue;

		for ( int item = ( index == iFirst ) ? iCursor % VGUI_SCREEN_QUEUE_STRIDE : 0; item < pScreen->KeptItems(); item++ )
		{
			const int iSize = pScreen->SendKept( NULL, item );
			if ( !iSize )
				continue;	// not set
			if ( iSize > iBudget && iBudget < VGUI_SCREEN_QUEUE_BYTES )
			{
				iCursor = index * VGUI_SCREEN_QUEUE_STRIDE + item;
				return FALSE;
			}
			pScreen->SendKept( pPlayer, item );
			iBudget -= iSize;
		}
	}
	return TRUE;
}

static SendQueue_Sender s_ScreenSender( SENDQUEUE_WORLDSCREEN, CVGuiScreen::SendQueued, 0, VGUI_SCREEN_QUEUE_DELAY,
	VGUI_SCREEN_QUEUE_INTERVAL, VGUI_SCREEN_QUEUE_BYTES );

// Standing in front of the screen: within inputdistance of its plane (front side) and of its rectangle. Source
// measured from the screen's corner, which is wrong for wide screens.
// Bound mode: within inputdistance of the carrier's box (the client checks the front side of the faces).
BOOL CVGuiScreen::InReach( CBasePlayer *pPlayer )
{
	if ( m_bBound )
	{
		CBaseEntity *pCarrier = Carrier();
		if ( !pCarrier )
			return FALSE;
		const Vector eye = pPlayer->pev->origin + pPlayer->pev->view_ofs;
		const float reach = m_flInputDistance + VGUI_SCREEN_REACH_SLACK;
		for ( int i = 0; i < 3; i++ )
		{
			if ( eye[i] < pCarrier->pev->absmin[i] - reach || eye[i] > pCarrier->pev->absmax[i] + reach )
				return FALSE;
		}
		return TRUE;
	}

	UTIL_MakeVectors( pev->angles );
	const Vector normal = gpGlobals->v_forward;
	const Vector right = -gpGlobals->v_right;	// the viewer's right: the screen's width
	const Vector up = gpGlobals->v_up;

	const Vector rel = pPlayer->pev->origin + pPlayer->pev->view_ofs - pev->origin;
	const float reach = m_flInputDistance + VGUI_SCREEN_REACH_SLACK;
	const float dist = DotProduct( rel, normal );
	if ( dist < -VGUI_SCREEN_REACH_SLACK || dist > reach )
		return FALSE;
	const float x = DotProduct( rel, right );
	const float y = DotProduct( rel, up );
	return x >= -reach && x <= m_flWidth + reach && y >= -reach && y <= m_flHeight + reach;
}

void CVGuiScreen::PlayerCommand( CBasePlayer *pPlayer, const char *command )
{
	if ( ( pev->spawnflags & SF_VGUI_SCREEN_INPUT_DISABLED ) || IsMovie() || !m_bActive || !pPlayer->IsAlive() || pPlayer->IsObserver() || pPlayer->m_bSpectating || !InReach( pPlayer ) )
		return;

	const int index = pPlayer->entindex();
	if ( index >= 1 && index < (int)ARRAYSIZE( s_flNextCommand ) )
	{
		// a new map starts the clock again
		if ( s_flNextCommand[ index ] > gpGlobals->time + VGUI_SCREEN_COMMAND_INTERVAL )
			s_flNextCommand[ index ] = 0;
		if ( gpGlobals->time < s_flNextCommand[ index ] )
			return;
		s_flNextCommand[ index ] = gpGlobals->time + VGUI_SCREEN_COMMAND_INTERVAL;
	}

	ALERT( at_aiconsole, "vgui_screen \"%s\": \"%s\" from %s\n", STRING( pev->targetname ), command,
		STRING( pPlayer->pev->netname ) );

	if ( !FStringNull( pev->target ) )
		FireTargets( STRING( pev->target ), pPlayer, this, USE_TOGGLE, 0 );

	for ( int i = 0; i < m_iCommandTargets; i++ )
	{
		if ( !stricmp( STRING( m_iszCommands[i] ), command ) )
			FireTargets( STRING( m_iszCommandTargets[i] ), pPlayer, this, USE_TOGGLE, 0 );
	}
}

void VGuiScreen_ClientCommand( CBasePlayer *pPlayer )
{
	if ( CMD_ARGC() != 3 )
		return;
	const int index = atoi( CMD_ARGV( 1 ) );
	const char *command = CMD_ARGV( 2 );
	// a plain command: never run, only compared with the map's "~<command>" keys
	if ( index <= gpGlobals->maxClients || index >= gpGlobals->maxEntities ||
		 !WScreen_ValidName( command, WSCREEN_MAX_COMMAND, 1 ) )
		return;

	edict_t *pEdict = INDEXENT( index );
	if ( FNullEnt( pEdict ) || pEdict->free || !FClassnameIs( pEdict, "vgui_screen" ) )
		return;
	CVGuiScreen *pScreen = (CVGuiScreen *)CBaseEntity::Instance( pEdict );
	if ( pScreen )
		pScreen->PlayerCommand( pPlayer, command );
}

//=========================================================
// vgui_control_proxy: when fired, changes one control of the screen(s) named "screen" for everybody: its text
// (or value: a ProgressBar, AnalogBar or Slider takes the text as a percentage), enabled or visible state ("mode"),
// or plays a sequence of the layout's animation script (mode 5: "text" = the sequence, "controlname" = the state
// group it is kept under for players who join later, empty: not kept). Replaces Source's SetControlText /
// SetControlEnabled / SetControlVisible inputs and the proxy's inputs (GoldSrc's Use carries no text). The text
// is read when fired, so trigger_changevalue / trigger_copyvalue can write it first.
//=========================================================

#define VGUI_PROXY_MODE_TEXT	0
#define VGUI_PROXY_MODE_ENABLE	1
#define VGUI_PROXY_MODE_DISABLE	2
#define VGUI_PROXY_MODE_SHOW	3
#define VGUI_PROXY_MODE_HIDE	4
#define VGUI_PROXY_MODE_ANIMATE	5

class CVGuiControlProxy : public CPointEntity
{
public:
	void	KeyValue( KeyValueData *pkvd );
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

private:
	string_t	m_iszScreen;		// "screen"
	string_t	m_iszControl;		// "controlname"
	string_t	m_iszText;			// "text"
	int			m_iMode;			// "mode"
};

LINK_ENTITY_TO_CLASS( vgui_control_proxy, CVGuiControlProxy );

TYPEDESCRIPTION	CVGuiControlProxy::m_SaveData[] =
{
	DEFINE_FIELD( CVGuiControlProxy, m_iszScreen, FIELD_STRING ),
	DEFINE_FIELD( CVGuiControlProxy, m_iszControl, FIELD_STRING ),
	DEFINE_FIELD( CVGuiControlProxy, m_iszText, FIELD_STRING ),
	DEFINE_FIELD( CVGuiControlProxy, m_iMode, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CVGuiControlProxy, CPointEntity );

void CVGuiControlProxy::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "screen" ) )
	{
		m_iszScreen = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "controlname" ) )
	{
		m_iszControl = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "text" ) )
	{
		m_iszText = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "mode" ) )
	{
		m_iMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CVGuiControlProxy::Spawn( void )
{
	if ( m_iMode == VGUI_PROXY_MODE_ANIMATE )
	{
		if ( FStringNull( m_iszText ) || !WScreen_ValidName( STRING( m_iszText ), WSCREEN_MAX_CONTROLNAME, 0 ) )
			ALERT( at_console, "vgui_control_proxy \"%s\": \"text\" (the sequence) missing or not a plain name\n",
				STRING( pev->targetname ) );
		if ( !FStringNull( m_iszControl ) && STRING( m_iszControl )[0] &&
			 !WScreen_ValidName( STRING( m_iszControl ), WSCREEN_MAX_CONTROLNAME, 0 ) )
			ALERT( at_console, "vgui_control_proxy \"%s\": \"controlname\" (the state group) not a plain name\n",
				STRING( pev->targetname ) );
	}
	else if ( FStringNull( m_iszControl ) || !WScreen_ValidName( STRING( m_iszControl ), WSCREEN_MAX_CONTROLNAME, 0 ) )
		ALERT( at_console, "vgui_control_proxy \"%s\": \"controlname\" missing or not a plain name: does nothing\n",
			STRING( pev->targetname ) );
	if ( !FStringNull( m_iszText ) && strlen( STRING( m_iszText ) ) >= WSCREEN_MAX_VALUE )
	{
		ALERT( at_console, "vgui_control_proxy \"%s\": \"text\" longer than %d characters, cut\n",
			STRING( pev->targetname ), WSCREEN_MAX_VALUE - 1 );
		char text[ WSCREEN_MAX_VALUE ];
		strncpy( text, STRING( m_iszText ), sizeof( text ) - 1 );
		text[ sizeof( text ) - 1 ] = 0;
		m_iszText = ALLOC_STRING( text );
	}
	CPointEntity::Spawn();
}

void CVGuiControlProxy::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FStringNull( m_iszScreen ) )
		return;
	const bool hasControl = !FStringNull( m_iszControl ) && STRING( m_iszControl )[0];
	if ( m_iMode == VGUI_PROXY_MODE_ANIMATE )
	{
		if ( FStringNull( m_iszText ) || !WScreen_ValidName( STRING( m_iszText ), WSCREEN_MAX_CONTROLNAME, 0 ) ||
			 ( hasControl && !WScreen_ValidName( STRING( m_iszControl ), WSCREEN_MAX_CONTROLNAME, 0 ) ) )
			return;
	}
	else if ( !hasControl || !WScreen_ValidName( STRING( m_iszControl ), WSCREEN_MAX_CONTROLNAME, 0 ) )
		return;

	// a text written by trigger_changevalue / trigger_copyvalue may be longer than the message allows
	if ( !FStringNull( m_iszText ) && strlen( STRING( m_iszText ) ) >= WSCREEN_MAX_VALUE )
	{
		char text[ WSCREEN_MAX_VALUE ];
		strncpy( text, STRING( m_iszText ), sizeof( text ) - 1 );
		text[ sizeof( text ) - 1 ] = 0;
		m_iszText = ALLOC_STRING( text );
	}

	int op;
	string_t val;
	switch ( m_iMode )
	{
	case VGUI_PROXY_MODE_ENABLE:	op = WSCREEN_CONTROL_ENABLED;	val = MAKE_STRING( "1" ); break;
	case VGUI_PROXY_MODE_DISABLE:	op = WSCREEN_CONTROL_ENABLED;	val = MAKE_STRING( "0" ); break;
	case VGUI_PROXY_MODE_SHOW:		op = WSCREEN_CONTROL_VISIBLE;	val = MAKE_STRING( "1" ); break;
	case VGUI_PROXY_MODE_HIDE:		op = WSCREEN_CONTROL_VISIBLE;	val = MAKE_STRING( "0" ); break;
	case VGUI_PROXY_MODE_ANIMATE:	op = WSCREEN_CONTROL_ANIMATION;	val = m_iszText; break;
	default:						op = WSCREEN_CONTROL_TEXT;		val = m_iszText; break;
	}

	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( m_iszScreen ) ) ) != NULL )
	{
		if ( FClassnameIs( pEntity->pev, "vgui_screen" ) )
			( (CVGuiScreen *)pEntity )->SetControl( m_iszControl, op, val );
	}
}
