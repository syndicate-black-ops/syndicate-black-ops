//=========================================================
// Tutor: Counter-Strike: Condition Zero's tutor window, driven by the map's env_tutor entities.
// docs/TUTOR_PORT.md (design, message format, mapper guide).
//
// The system is CZ's (ReGameDLL_CS regamedll/dlls/tutor*.{h,cpp}): one tutor window per player showing ONE message
// at a time, the other messages waiting in a queue (priority, interrupt, lifetime), the TutorText / TutorClose
// messages, colours per message type and icons from tutorscheme.res. CZ's content (tutordata.txt, game event hooks)
// is not used: the game never shows a tutor message by itself, level designers place env_tutor entities and
// trigger them by name (like game_text / env_message).
//
// Every human player has his own window and queue; tutor messages are always shown (no client setting turns them
// off). Players in spectator mode get none.
//=========================================================

#ifndef TUTOR_H
#define TUTOR_H

class CBaseEntity;
class CBasePlayer;

#define TUTOR_MAX_PLAYERS			32

// env_tutor spawnflags
#define SF_TUTOR_ACTIVATOR_ONLY		1	// only the activator (a player) sees it; default: all players
#define SF_TUTOR_ALIVE_ONLY			2	// SBO 2.0: always on (CEnvTutor::CanShowTo: only players alive and in play); kept for maps that set it
#define SF_TUTOR_ON_SPAWN			4	// shown to every player when he spawns ("start on", map intros)
#define SF_TUTOR_REPEATABLE			8	// a player can see it again (up to "maxshows"); default: once per player

// The "type" short of TutorText (CZ's TutorMessageType): the client takes the text colour from tutorscheme.res
// "TutorTextClass<type>" (no class: "TutorTextClassDefault"), the default icon and the default sound from it
enum TutorMessageType
{
	TUTORMESSAGETYPE_DEFAULT      = (1 << 0), // green  | icon i     | tutor_msg.wav
	TUTORMESSAGETYPE_FRIEND_DEATH = (1 << 1), // red    | icon skull | friend_died.wav
	TUTORMESSAGETYPE_ENEMY_DEATH  = (1 << 2), // blue   | icon skull | enemy_died.wav
	TUTORMESSAGETYPE_SCENARIO     = (1 << 3), // yellow | icon i     | tutor_msg.wav
	TUTORMESSAGETYPE_BUY          = (1 << 4), // green
	TUTORMESSAGETYPE_CAREER       = (1 << 5), // green
	TUTORMESSAGETYPE_HINT         = (1 << 6), // green
	TUTORMESSAGETYPE_INGAME_HINT  = (1 << 7), // green
	TUTORMESSAGETYPE_END_GAME     = (1 << 8), // yellow
};

// TutorText's icon byte (the client's preset icons under ui/gfx/career/)
enum TutorIcon
{
	TUTOR_ICON_NONE = 0,
	TUTOR_ICON_I,				// icon_i (tinted)
	TUTOR_ICON_EXCLAMATION,		// icon_! (tinted)
	TUTOR_ICON_SKULLS,			// icon_skulls (tinted)
	TUTOR_ICON_I_BIGGER,		// icon_i-bigger (own colours)
	TUTOR_ICON_EXCLAMATION_BIGGER,	// icon_!-bigger (own colours)
	TUTOR_ICON_CUSTOM = 255,	// a path follows (a map's .tga)
};

// TutorText's flags byte
#define TUTOR_FLAG_DEAD				(1 << 0)	// the player is dead or observing (window below the spectator bars)
#define TUTOR_FLAG_TEXT_COLOR		(1 << 1)	// r g b follow
#define TUTOR_FLAG_ICON_COLOR		(1 << 2)	// r g b follow
#define TUTOR_FLAG_BG_COLOR			(1 << 3)	// r g b a follow
#define TUTOR_FLAG_BORDER_COLOR		(1 << 4)	// r g b a follow

// TutorText's alignment byte (env_tutor "align")
enum TutorAlign
{
	TUTOR_ALIGN_TOP_RIGHT = 0,	// CZ
	TUTOR_ALIGN_TOP_LEFT,
	TUTOR_ALIGN_TOP_CENTER,
	TUTOR_ALIGN_CENTER_LEFT,
	TUTOR_ALIGN_CENTER,
	TUTOR_ALIGN_CENTER_RIGHT,
	TUTOR_ALIGN_BOTTOM_LEFT,
	TUTOR_ALIGN_BOTTOM_CENTER,
	TUTOR_ALIGN_BOTTOM_RIGHT,
	TUTOR_ALIGN_COUNT
};

// env_tutor's settings (saved as a block, like game_text's hudtextparms_t)
typedef struct
{
	int		type;				// TutorMessageType
	int		icon;				// TutorIcon; -1 = the type's (skull for the death types, else i)
	int		sound;				// 0 = the type's, 1 = none, 2 = tutor_msg, 3 = friend_died, 4 = enemy_died
	int		align;				// TutorAlign
	float	x, y;				// 0..1 = position on the screen, -1 = from the alignment
	float	holdtime;			// seconds shown; 0 = until triggered off (or replaced)
	float	lifetime;			// seconds it may wait in the queue; 0 = no limit, -1 = never waits
	int		priority;
	int		interrupt;			// replaces a shown message of the same or a lower priority at once
	int		maxshows;			// per player with SF_TUTOR_REPEATABLE; 0 = no limit
	int		colorFlags;			// TUTOR_FLAG_*_COLOR: the colours below are set
	byte	textColor[4];
	byte	iconColor[4];
	byte	bgColor[4];
	byte	borderColor[4];
} tutorparms_t;

//=========================================================
// env_tutor: a tutor message for level designers (see the FGD / docs/TUTOR_PORT.md)
//=========================================================
class CEnvTutor : public CPointEntity
{
public:
	CEnvTutor( void );

	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	// May this player be shown the message now (filters, maxshows)?
	BOOL	CanShowTo( CBasePlayer *pPlayer );
	void	RegisterShown( int playerIndex );
	void	ForgetPlayer( int playerIndex );

	// Network values
	int		IconId( void );
	const char *IconPath( void );	// TUTOR_ICON_CUSTOM: the .tga (without the extension)
	const char *SoundPath( void );	// "" = none

	tutorparms_t	m_parms;
	string_t		m_iszIconFile;		// "iconfile": a map's .tga
	string_t		m_iszSoundFile;		// "soundfile": a map's .wav
	string_t		m_iszCloseTarget;	// "closetarget": fired when the message closes
	string_t		m_iszMaster;		// "master"

private:
	// times each player was shown it (this map), index = player entity index
	int		m_timesShown[ TUTOR_MAX_PLAYERS + 1 ];
};

// Queues the message for a player (the window shows one message at a time). False if it can't be shown to him
// (always for a player who is not alive and in play: dead, waiting, in the menus, spectating).
BOOL Tutor_Show( CBasePlayer *pPlayer, CEnvTutor *pTutor );
// Closes the message for a player (if shown) and takes it out of his queue
void Tutor_Close( CBasePlayer *pPlayer, CEnvTutor *pTutor );

// StartFrame: the players' queues and display times
void Tutor_StartFrame( void );
// ClientPutInServer (every map): a fresh window and queue
void Tutor_ClientPutInServer( CBasePlayer *pPlayer );
// ServerDeactivate (map change / shutdown)
void Tutor_ServerDeactivate( void );
// ClientDisconnect: the slot's queue and env_tutor show counts
void Tutor_ClientDisconnected( edict_t *pClient );
// CBasePlayer::Spawn: the env_tutor entities with SF_TUTOR_ON_SPAWN (nothing for a spawn that leaves the player
// out of play: the one at connect time, a waiting player's)
void Tutor_PlayerSpawned( CBasePlayer *pPlayer );

#endif // TUTOR_H
