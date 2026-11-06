//=========================================================
// game_timer: a HUD countdown timer for level designers. Port of SBO Source's game_timer (Mapbase's "generic
// custom timer entity" by Blixibon; syndicate-blackops-source src/game/shared/game_timer.{h,cpp}): the same
// keyvalues, defaults and timing rules. docs/GAME_TIMER.md (mapping of Source's inputs and outputs, message,
// mapper guide).
//
// The timer counts on the server; clients get its state in the GameTimer message when it changes (and with their
// HUD initialisation) and count down by themselves (src/cl_dll/vgui2/hud_game_timer.cpp).
//=========================================================

#ifndef GAME_TIMER_H
#define GAME_TIMER_H

class CBasePlayer;

#define MAX_GAME_TIMER_CAPTION			128		// Source: with the terminator
#define MAX_GAME_TIMER_CAPTION_LINES	4

// Source: sv_game_menu_default_warn_frac (0.25). The part of the timer's length at which the warning state begins
// when "warn_time" is negative.
#define GAME_TIMER_DEFAULT_WARN_FRAC	0.25f

// GameTimer's flags byte
#define GAME_TIMER_FLAG_ENABLED			(1 << 0)	// 0: the timer is disabled or hidden (the message ends here)
#define GAME_TIMER_FLAG_PAUSED			(1 << 1)	// the time is the seconds remaining; else the server time it ends at
#define GAME_TIMER_FLAG_SHOW_REMAINING	(1 << 2)	// else the elapsed time is shown
#define GAME_TIMER_FLAG_FULL			(1 << 3)	// the display settings and the caption follow the time

// What happened, for the targets (Source's outputs). Fired once the timer's state is complete and sent, as
// Source's queued outputs are.
enum GameTimerEvent
{
	GAME_TIMER_EVENT_FINISHED = 0,	// "target"
	GAME_TIMER_EVENT_PAUSED,		// "pausetarget"
	GAME_TIMER_EVENT_RESUMED,		// "resumetarget"
	GAME_TIMER_EVENT_WARNED,		// "warntarget"
};

#define GAME_TIMER_MAX_EVENTS			8

class CGameTimer : public CPointEntity
{
public:
	CGameTimer( void );

	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	EXPORT TimerThink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	// Source's accessors
	float	GetTimeRemaining( void );		// seconds left, paused or not; never negative
	float	GetTimerMaxLength( void );		// "max_length", or "timer_length" without one
	float	GetWarnTime( void );			// "warn_time", or the default part of the length
	BOOL	IsDisabled( void ) { return m_bIsDisabled; }
	BOOL	IsTimerPaused( void ) { return m_bTimerPaused; }
	// Clients know the timer: enabled and "show_in_hud"
	BOOL	IsShown( void ) { return !m_bIsDisabled && m_bShowInHUD; }

	// The current value of every key of the timer (Source: CGameTimer::GetKeyValue), for trigger_changevalue /
	// trigger_copyvalue: "time_remaining" is the time left, "paused" the pause state.
	virtual CKeyValue	GetKeyValue( const char *keyName );

	// The GameTimer message: to everybody (pPlayer NULL) or to one player
	void	SendState( CBasePlayer *pPlayer, BOOL full );

private:
	// Source's inputs
	void	Enable( CBaseEntity *pActivator );
	void	Disable( CBaseEntity *pActivator );
	void	SetTimeRemaining( float flTime );
	void	AddTimerSeconds( float flTimeToAdd );
	void	SetMaxTime( float flTime );
	void	PauseTimer( CBaseEntity *pActivator );
	void	ResumeTimer( CBaseEntity *pActivator );

	// One step of the countdown. TRUE: the timer finished.
	BOOL	RunTimer( void );

	// A key's new value. 0: not the timer's key; else KEY_* (game_timer.cpp)
	int		ApplyKey( const char *szKeyName, const char *szValue );
	// The clients' state after a change: wasShown = IsShown() before it
	void	SyncClients( BOOL wasShown, BOOL full );

	void	QueueEvent( int event, CBaseEntity *pActivator );
	void	FireEvents( void );

	// Keyvalues (Source's names)
	float		m_flTimerInitialLength;		// "timer_length"
	float		m_flTimerMaxLength;			// "max_length"
	float		m_flWarnTime;				// "warn_time"
	BOOL		m_bIsDisabled;				// "StartDisabled"
	BOOL		m_bStartPaused;				// "start_paused"
	BOOL		m_bShowTimeRemaining;		// "show_time_remaining"
	BOOL		m_bDisableOnFinish;			// "disable_on_finish"
	BOOL		m_bShowInHUD;				// "show_in_hud"
	int			m_nProgressBarMaxSegments;	// "progress_bar_max"
	int			m_nProgressBarOverride;		// "progress_bar_override"
	float		m_flOverrideX;				// "x"
	float		m_flOverrideY;				// "y"
	string_t	m_iszTimerCaption;			// "timer_caption"
	int			m_nCaptionWidth;			// "caption_width"
	int			m_nCaptionMaxLines;			// "caption_lines"
	string_t	m_iszPlayerFilterName;		// "PlayerFilter": kept, not used (docs/GAME_TIMER.md)

	// Source's outputs other than OnFinished ("target")
	string_t	m_iszPauseTarget;			// "pausetarget"
	string_t	m_iszResumeTarget;			// "resumetarget"
	string_t	m_iszWarnTarget;			// "warntarget"

	// State
	float		m_flTimerEndTime;			// running: the server time it reaches zero
	float		m_flTimeRemaining;			// paused: the seconds left
	BOOL		m_bTimerPaused;
	BOOL		m_bStartedWarn;
	BOOL		m_bSpawned;					// keyvalues from now on are changes to a live timer

	// Events waiting for FireEvents
	int			m_iEventCount;
	int			m_iEvents[ GAME_TIMER_MAX_EVENTS ];
	EHANDLE		m_hEventActivators[ GAME_TIMER_MAX_EVENTS ];
};

// CBasePlayer::UpdateClientData, with every HUD initialisation: the timers a client has to know (enabled, shown in
// the HUD). Also reaches clients who joined as spectators or sit in the menus, who are never spawned.
void GameTimer_SendAll( CBasePlayer *pPlayer );

#endif // GAME_TIMER_H
