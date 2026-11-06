//=========================================================
// vgui_screen: a VGUI2 panel shown on a rectangle in the world, which players can aim at and click (SBO Source's
// vgui_screen, reimplemented for GoldSrc; docs/WORLD_SCREENS.md). vgui_control_proxy changes a control on one.
//
// The server knows where the screens are and what they show (a layout name); each client builds and draws the panel
// itself (src/cl_dll/worldscreen/). Clients get every screen in the WScreen message after their HUD was
// (re)initialised, through the shared per-client send queue (client_sendqueue.h), and the changes as they happen. A
// button's command comes back as the client command "vgui_screen_command <entindex> <command>", which fires the
// screen's targets.
//=========================================================

#ifndef VGUI_SCREEN_H
#define VGUI_SCREEN_H

class CBasePlayer;

// spawnflags
#define SF_VGUI_SCREEN_START_INACTIVE	1
#define SF_VGUI_SCREEN_INPUT_DISABLED	2	// display only: no aim, cursor or clicks; its commands are refused
#define SF_VGUI_SCREEN_MOVIE_SOUND2D	4	// movie: its sound at the same volume everywhere (no falloff, no direction)
#define SF_VGUI_SCREEN_MOVIE_NOSOUND	8	// movie: no sound

#define VGUI_SCREEN_DEFAULT_MOVIE_RADIUS	1250	// "radius" (Source's ambient_generic "Max Audible Distance")

#define VGUI_SCREEN_MAX_COMMAND_TARGETS	16	// "~<command>" keys per screen
#define VGUI_SCREEN_MAX_CONTROLS		64	// controls a screen keeps the latest state of, for players who join later
#define VGUI_SCREEN_MAX_ANIM_GROUPS		16	// animation state groups it keeps the latest sequence of

// m_iControlFlags: which properties of a kept control the map has set, and the set enabled / visible values
#define VGUI_CONTROL_HAS_TEXT		1	// text or value (WSCREEN_CONTROL_TEXT)
#define VGUI_CONTROL_HAS_ENABLED	2
#define VGUI_CONTROL_ENABLED		4
#define VGUI_CONTROL_HAS_VISIBLE	8
#define VGUI_CONTROL_VISIBLE		16

class CVGuiScreen : public CPointEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	Activate( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BOOL	IsActive( void ) { return m_bActive; }

	// The send queue's sender (SENDQUEUE_WORLDSCREEN): after a player's HUD was (re)initialised (entering the game,
	// "fullupdate"), every screen's DEFINE and kept state, from iCursor on while they fit in iBudget
	static int	SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget );
	// A vgui_control_proxy's change: sent to everybody; its latest state per control and property (and the latest
	// sequence per animation state group) is kept for players who join later
	void	SetControl( string_t control, int op, string_t value );
	// vgui_screen_command from a player: checked, then the targets are fired
	void	PlayerCommand( CBasePlayer *pPlayer, const char *command );

private:
	BOOL	InReach( CBasePlayer *pPlayer );
	// Bound mode: the carrier brush entity, or NULL (removed since)
	CBaseEntity	*Carrier( void ) { return m_bBound ? (CBaseEntity *)m_hCarrier : NULL; }
	void	SendState( void );
	int		DefineSize( void );						// the DEFINE message's size in bytes
	int		SendDefine( CBasePlayer *pPlayer );		// returns the message's size in bytes
	int		SendControl( CBasePlayer *pPlayer, string_t control, int op, const char *value );
	// one kept item to one player, returns its size (0 if it is not set); pPlayer NULL: only its size
	int		SendKept( CBasePlayer *pPlayer, int item );
	BOOL	KeptControl( int item, string_t &control, int &op, const char *&value );	// a kept CONTROL item, if set
	int		KeptItems( void ) { return 1 + 3 * m_iControls + m_iAnims; }	// the DEFINE, then the kept items

	string_t	m_iszPanelName;		// "panelname": ui/scripts/screens/<panelname>.res on the client
	// Movie screen (docs/WORLD_SCREENS.md, "Movie screens"; key names of Source's vgui_movie_display and
	// ambient_generic): "moviefilename" an Ogg Theora video in video/, "looping", "health" volume 0-10, "radius"
	string_t	m_iszMovie;
	BOOL		m_bLooping;
	float		m_flRadius;
	float		m_flMovieStart;		// gpGlobals->time it started (became active)
	BOOL		IsMovie( void ) { return !FStringNull( m_iszMovie ); }
	int			MovieVolume( void );
	float		m_flWidth;			// "width", world units
	float		m_flHeight;			// "height"
	float		m_flInputDistance;	// "inputdistance"
	BOOL		m_bNoCursor;		// "showcursor" 0: no cursor while aimed at it (default -1 and 1: the cursor)
	BOOL		m_bActive;
	string_t	m_iszSurface;		// "surface": targetname of the carrier brush entity (bound mode; docs/WORLD_SCREENS.md)
	BOOL		m_bBound;			// "surface" named a brush entity (found in Activate): shown on its faces
	EHANDLE		m_hCarrier;			// that entity (NULL once it is removed: the screen is then not shown)

	// "~<command>" keys: the target fired for a button with that command
	int			m_iCommandTargets;
	string_t	m_iszCommands[ VGUI_SCREEN_MAX_COMMAND_TARGETS ];
	string_t	m_iszCommandTargets[ VGUI_SCREEN_MAX_COMMAND_TARGETS ];

	// the latest state of each control the map changed (vgui_control_proxy)
	int			m_iControls;
	string_t	m_iszControlNames[ VGUI_SCREEN_MAX_CONTROLS ];
	string_t	m_iszControlTexts[ VGUI_SCREEN_MAX_CONTROLS ];
	int			m_iControlFlags[ VGUI_SCREEN_MAX_CONTROLS ];	// VGUI_CONTROL_*
	// the latest animation sequence per state group
	int			m_iAnims;
	string_t	m_iszAnimGroups[ VGUI_SCREEN_MAX_ANIM_GROUPS ];
	string_t	m_iszAnimSequences[ VGUI_SCREEN_MAX_ANIM_GROUPS ];
};

// ClientCommand "vgui_screen_command <entindex> <command>"
void VGuiScreen_ClientCommand( CBasePlayer *pPlayer );

#endif // VGUI_SCREEN_H
