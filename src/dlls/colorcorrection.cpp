//=========================================================
// SBO 2.0 color_correction / color_correction_volume: see colorcorrection.h and docs/COLOR_CORRECTION.md. Source SDK
// 2013 (game/server/colorcorrection.cpp, colorcorrectionvolume.cpp) for GoldSrc, with keys of its own (Source's
// names where Source has the key).
//
//   * color_correction: a point. On, its weight fades up to "maxweight" over "fadeInDuration"; off, down to 0 over
//     "fadeOutDuration". With "minfalloff" / "maxfalloff" (-1 / -1 = everywhere) the weight is full up to the min
//     falloff from it and gone from the max falloff.
//   * color_correction_volume: a brush. A player's view inside the brush fades its weight up to "maxweight" over
//     "fadeDuration", outside down to 0 (each client works it out for its own view).
//   * Both: a lookup table file (Source .raw, or .cube) under gfx/colorcorrection/, precached as a generic resource so
//     clients download it, and / or the simple keys (saturation, contrast, brightness, gamma, tint). Targetname
//     toggles (on / off / toggle), "master" switches it off while the master is not active, "StartDisabled",
//     "exclusive" (while it has weight it takes that share from the others).
//   * The server keeps only the state (on / off, where a point entity's fade is); every player gets every entity at
//     HUD init and again when one changes. The client works out the weights from its own view (spectators, the
//     picture-in-picture view and demos follow their camera) and grades the 3D view.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"saverestore.h"
#include	"colorcorrection.h"
#include	"colorcorrection_shared.h"

extern int gmsgColorCorr;

#define COLORCORR_MASTER_POLL	0.1f	// how often the master is looked at (and runtime key changes sent)

static float ClampTime( float f )
{
	if ( !( f > 0.0f ) )
		return 0.0f;
	return f > 655.0f ? 655.0f : f;
}

static float Clamp01( float f )
{
	if ( !( f > 0.0f ) )
		return 0.0f;
	return f > 1.0f ? 1.0f : f;
}

static int KeyShort( float value, float unit )
{
	if ( !( value == value ) )
		value = 0.0f;
	float v = value * unit;
	if ( v > 32767.0f )
		v = 32767.0f;
	else if ( v < -32767.0f )
		v = -32767.0f;
	return (int)( v < 0.0f ? v - 0.5f : v + 0.5f );
}

static int UnsignedShort( float value, float unit )
{
	float v = value * unit + 0.5f;
	if ( !( v > 0.0f ) )
		return 0;
	if ( v > 65535.0f )
		v = 65535.0f;
	return (int)(short)(unsigned short)(int)v;
}

static int CoordShort( float value )
{
	return KeyShort( value, 1.0f );
}

static int ColorByte( int value )
{
	return value < 0 ? 0 : ( value > 255 ? 255 : value );
}

//---------------------------------------------------------
// The keys and state both classes share
//---------------------------------------------------------
class CColorCorrectionBase : public CBaseEntity
{
public:
	CColorCorrectionBase();
	void	KeyValue( KeyValueData *pkvd );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	UpdateOnRemove( void );
	void	RuntimeKeyValueChanged( const char *keyName );
	void	EXPORT PollThink( void );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	virtual BOOL	IsVolume( void ) = 0;
	BOOL	Usable( void ) { return m_bUsable; }
	// sends this entity to one player (first: the first of a full list)
	void	SendTo( CBasePlayer *pPlayer, BOOL bFirst );

protected:
	void	SpawnCommon( void );
	void	SendToAll( void );
	BOOL	Enabled( void ) { return m_bOn && m_bMasterOn; }
	void	SetEnabled( BOOL bOn, BOOL bMasterOn, BOOL bFade );
	float	WeightNow( void );
	float	FadeInTime( void ) { return IsVolume() ? m_flFadeDuration : m_flFadeIn; }
	float	FadeOutTime( void ) { return IsVolume() ? m_flFadeDuration : m_flFadeOut; }

	string_t	m_iszFile;			// "filename": gfx/colorcorrection/<name>.raw or .cube ("" = none)
	string_t	m_iszMaster;		// "master"
	float		m_flMaxWeight;		// "maxweight" 0 .. 1
	float		m_flFadeIn;			// "fadeInDuration" (point)
	float		m_flFadeOut;		// "fadeOutDuration" (point)
	float		m_flFadeDuration;	// "fadeDuration" (volume)
	float		m_flMinFalloff;		// "minfalloff" (point; -1 = no falloff)
	float		m_flMaxFalloff;		// "maxfalloff"
	BOOL		m_bExclusive;		// "exclusive"
	float		m_flSaturation;		// "saturation" (1 = as it is, 0 = black and white)
	float		m_flContrast;		// "contrast" (1 = as it is)
	float		m_flBrightness;		// "brightness" (0 = as it is, added)
	float		m_flGamma;			// "gamma" (1 = as it is, higher = brighter midtones)
	int			m_iTint[3];			// "tintcolor"
	float		m_flTintAmount;		// "tintamount" 0 .. 1
	BOOL		m_bOn;				// on (Use), whatever the master says
	BOOL		m_bMasterOn;		// the master was active at the last look (TRUE without a master)
	float		m_flWeightStart;	// point: the weight when the last fade started
	float		m_flFadeStart;		// point: when it started
	BOOL		m_bUsable;			// the file is valid (or none)
	BOOL		m_bSpawned;			// "filename" can no longer change (it is precached)
	BOOL		m_bDirty;			// a key changed at run time: send at the next poll
};

TYPEDESCRIPTION CColorCorrectionBase::m_SaveData[] =
{
	DEFINE_FIELD( CColorCorrectionBase, m_iszFile, FIELD_STRING ),
	DEFINE_FIELD( CColorCorrectionBase, m_iszMaster, FIELD_STRING ),
	DEFINE_FIELD( CColorCorrectionBase, m_flMaxWeight, FIELD_FLOAT ),
	DEFINE_FIELD( CColorCorrectionBase, m_flFadeIn, FIELD_FLOAT ),
	DEFINE_FIELD( CColorCorrectionBase, m_flFadeOut, FIELD_FLOAT ),
	DEFINE_FIELD( CColorCorrectionBase, m_flFadeDuration, FIELD_FLOAT ),
	DEFINE_FIELD( CColorCorrectionBase, m_flMinFalloff, FIELD_FLOAT ),
	DEFINE_FIELD( CColorCorrectionBase, m_flMaxFalloff, FIELD_FLOAT ),
	DEFINE_FIELD( CColorCorrectionBase, m_bExclusive, FIELD_BOOLEAN ),
	DEFINE_FIELD( CColorCorrectionBase, m_flSaturation, FIELD_FLOAT ),
	DEFINE_FIELD( CColorCorrectionBase, m_flContrast, FIELD_FLOAT ),
	DEFINE_FIELD( CColorCorrectionBase, m_flBrightness, FIELD_FLOAT ),
	DEFINE_FIELD( CColorCorrectionBase, m_flGamma, FIELD_FLOAT ),
	DEFINE_ARRAY( CColorCorrectionBase, m_iTint, FIELD_INTEGER, 3 ),
	DEFINE_FIELD( CColorCorrectionBase, m_flTintAmount, FIELD_FLOAT ),
	DEFINE_FIELD( CColorCorrectionBase, m_bOn, FIELD_BOOLEAN ),
	DEFINE_FIELD( CColorCorrectionBase, m_bMasterOn, FIELD_BOOLEAN ),
	DEFINE_FIELD( CColorCorrectionBase, m_flWeightStart, FIELD_FLOAT ),
	DEFINE_FIELD( CColorCorrectionBase, m_flFadeStart, FIELD_TIME ),
	DEFINE_FIELD( CColorCorrectionBase, m_bUsable, FIELD_BOOLEAN ),
	DEFINE_FIELD( CColorCorrectionBase, m_bSpawned, FIELD_BOOLEAN ),
};

// the keys' defaults (the constructor runs before the map's keys)
CColorCorrectionBase::CColorCorrectionBase()
{
	m_flMaxWeight = 1.0f;
	m_flFadeDuration = 1.0f;
	m_flMinFalloff = m_flMaxFalloff = -1.0f;
	m_flSaturation = m_flContrast = m_flGamma = 1.0f;
	m_iTint[0] = m_iTint[1] = m_iTint[2] = 255;
	m_bOn = TRUE;
	m_bMasterOn = TRUE;
}

int CColorCorrectionBase::Save( CSave &save )
{
	if ( !CBaseEntity::Save( save ) )
		return 0;
	return save.WriteFields( "CColorCorrectionBase", this, m_SaveData, ARRAYSIZE( m_SaveData ) );
}

int CColorCorrectionBase::Restore( CRestore &restore )
{
	if ( !CBaseEntity::Restore( restore ) )
		return 0;
	if ( !restore.ReadFields( "CColorCorrectionBase", this, m_SaveData, ARRAYSIZE( m_SaveData ) ) )
		return 0;
	Precache();	// a restored game precaches the file again (Spawn is not called)
	return 1;
}

void CColorCorrectionBase::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "filename" ) )
	{
		if ( m_bSpawned )
			ALERT( at_console, "%s \"%s\": its file can't be changed at run time (it would need a precache)\n",
				STRING( pev->classname ), STRING( pev->targetname ) );
		else if ( !pkvd->szValue[0] )
			m_iszFile = iStringNull;
		else
		{
			// a bare name is in gfx/colorcorrection/
			char path[ COLORCORR_MAX_PATH * 2 ];
			if ( strchr( pkvd->szValue, '/' ) )
				snprintf( path, sizeof( path ), "%s", pkvd->szValue );
			else
				snprintf( path, sizeof( path ), "%s%s", COLORCORR_FOLDER, pkvd->szValue );
			m_iszFile = ALLOC_STRING( path );
		}
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "master" ) )
	{
		m_iszMaster = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "maxweight" ) )
	{
		m_flMaxWeight = Clamp01( atof( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "StartDisabled" ) )
	{
		// a key read before Spawn only
		if ( !m_bSpawned )
			m_bOn = atoi( pkvd->szValue ) == 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "exclusive" ) )
	{
		m_bExclusive = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "saturation" ) )
	{
		m_flSaturation = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "contrast" ) )
	{
		m_flContrast = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "brightness" ) )
	{
		m_flBrightness = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gamma" ) )
	{
		m_flGamma = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "tintcolor" ) )
	{
		int r = 255, g = 255, b = 255;
		if ( sscanf( pkvd->szValue, "%d %d %d", &r, &g, &b ) == 3 )
		{
			m_iTint[0] = ColorByte( r );
			m_iTint[1] = ColorByte( g );
			m_iTint[2] = ColorByte( b );
		}
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "tintamount" ) )
	{
		m_flTintAmount = Clamp01( atof( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CColorCorrectionBase::Precache( void )
{
	m_bUsable = FStringNull( m_iszFile ) || ColorCorr_ValidPath( STRING( m_iszFile ) );
	if ( !m_bUsable )
	{
		ALERT( at_error, "%s \"%s\": bad file \"%s\" (a .raw or .cube in %s, at most %d characters)\n",
			STRING( pev->classname ), STRING( pev->targetname ), STRING( m_iszFile ), COLORCORR_FOLDER,
			COLORCORR_MAX_PATH - 1 );
		return;
	}

	// the mapper's table, precached by the entity that uses it: a file the client downloads
	if ( !FStringNull( m_iszFile ) )
		PRECACHE_GENERIC( (char *)STRING( m_iszFile ) );
}

void CColorCorrectionBase::SpawnCommon( void )
{
	Precache();
	m_bSpawned = TRUE;
	m_bMasterOn = TRUE;

	// on at the map start: at its full weight at once (Source CColorCorrection::Spawn), no fade
	m_flWeightStart = Enabled() ? m_flMaxWeight : 0.0f;
	m_flFadeStart = gpGlobals->time;

	// the master is looked at from the first frame on (the entities it names may spawn after this one)
	if ( !FStringNull( m_iszMaster ) )
	{
		SetThink( &CColorCorrectionBase::PollThink );
		pev->nextthink = gpGlobals->time + COLORCORR_MASTER_POLL;
	}
}

float CColorCorrectionBase::WeightNow( void )
{
	const BOOL bEnabled = Enabled();
	const float target = bEnabled ? m_flMaxWeight : 0.0f;
	const float time = bEnabled ? FadeInTime() : FadeOutTime();
	const float dt = gpGlobals->time - m_flFadeStart;
	if ( !( time > 0.0f ) || !( m_flMaxWeight > 0.0f ) )
		return target;
	if ( !( dt > 0.0f ) )
		return m_flWeightStart;
	const float step = m_flMaxWeight / time * dt;
	if ( m_flWeightStart < target )
		return m_flWeightStart + step >= target ? target : m_flWeightStart + step;
	return m_flWeightStart - step <= target ? target : m_flWeightStart - step;
}

// The state changes; the fade goes on from where it is (bFade FALSE: at its end at once)
void CColorCorrectionBase::SetEnabled( BOOL bOn, BOOL bMasterOn, BOOL bFade )
{
	const float flNow = WeightNow();
	m_bOn = bOn;
	m_bMasterOn = bMasterOn;
	m_flWeightStart = bFade ? flNow : ( Enabled() ? m_flMaxWeight : 0.0f );
	m_flFadeStart = gpGlobals->time;
}

void CColorCorrectionBase::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_bOn ) )
		return;
	SetEnabled( !m_bOn, m_bMasterOn, TRUE );
	SendToAll();
}

void CColorCorrectionBase::PollThink( void )
{
	pev->nextthink = gpGlobals->time + COLORCORR_MASTER_POLL;

	if ( !FStringNull( m_iszMaster ) )
	{
		const BOOL bMasterOn = UTIL_IsMasterTriggered( m_iszMaster, NULL ) ? TRUE : FALSE;
		if ( bMasterOn != m_bMasterOn )
		{
			SetEnabled( m_bOn, bMasterOn, TRUE );
			m_bDirty = TRUE;
		}
	}

	if ( m_bDirty )
	{
		m_bDirty = FALSE;
		SendToAll();
	}

	if ( FStringNull( m_iszMaster ) )
		pev->nextthink = 0;
}

// trigger_changevalue / trigger_copyvalue changed a key (keyvalue.cpp KeyValue_DispatchRuntime): sent with the next
// poll (several keys changed in one frame make one message)
void CColorCorrectionBase::RuntimeKeyValueChanged( const char *keyName )
{
	// a point entity's fade goes on from where it is with the new weight and times
	if ( FStrEq( keyName, "maxweight" ) && m_flWeightStart > m_flMaxWeight )
		m_flWeightStart = m_flMaxWeight;
	m_bDirty = TRUE;
	SetThink( &CColorCorrectionBase::PollThink );
	if ( pev->nextthink <= 0.0f || pev->nextthink > gpGlobals->time + COLORCORR_MASTER_POLL )
		pev->nextthink = gpGlobals->time + COLORCORR_MASTER_POLL;
}

void CColorCorrectionBase::UpdateOnRemove( void )
{
	// killtarget: every client forgets it
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->m_fGameHUDInitialized && !pPlayer->has_disconnected )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgColorCorr, NULL, pPlayer->pev );
				WRITE_SHORT( entindex() );
				WRITE_BYTE( COLORCORR_FLAG_REMOVED );
			MESSAGE_END();
		}
	}
	m_bUsable = FALSE;
	CBaseEntity::UpdateOnRemove();
}

void CColorCorrectionBase::SendTo( CBasePlayer *pPlayer, BOOL bFirst )
{
	if ( !m_bUsable || !pPlayer )
		return;

	int flags = 0;
	if ( Enabled() )
		flags |= COLORCORR_FLAG_ENABLED;
	if ( IsVolume() )
		flags |= COLORCORR_FLAG_VOLUME;
	if ( m_bExclusive )
		flags |= COLORCORR_FLAG_EXCLUSIVE;
	if ( bFirst )
		flags |= COLORCORR_FLAG_FIRST;

	MESSAGE_BEGIN( MSG_ONE, gmsgColorCorr, NULL, pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_BYTE( flags );
		WRITE_SHORT( KeyShort( m_flMaxWeight, COLORCORR_WEIGHT_UNIT ) );
		WRITE_SHORT( IsVolume() ? 0 : KeyShort( WeightNow(), COLORCORR_WEIGHT_UNIT ) );
		WRITE_SHORT( UnsignedShort( FadeInTime(), COLORCORR_TIME_UNIT ) );
		WRITE_SHORT( UnsignedShort( FadeOutTime(), COLORCORR_TIME_UNIT ) );
		if ( IsVolume() )
		{
			WRITE_SHORT( pev->modelindex );
			WRITE_SHORT( CoordShort( pev->origin.x ) );
			WRITE_SHORT( CoordShort( pev->origin.y ) );
			WRITE_SHORT( CoordShort( pev->origin.z ) );
		}
		else
		{
			WRITE_SHORT( CoordShort( pev->origin.x ) );
			WRITE_SHORT( CoordShort( pev->origin.y ) );
			WRITE_SHORT( CoordShort( pev->origin.z ) );
			WRITE_SHORT( CoordShort( m_flMinFalloff ) );
			WRITE_SHORT( CoordShort( m_flMaxFalloff ) );
		}
		WRITE_SHORT( KeyShort( m_flSaturation, COLORCORR_KEY_UNIT ) );
		WRITE_SHORT( KeyShort( m_flContrast, COLORCORR_KEY_UNIT ) );
		WRITE_SHORT( KeyShort( m_flBrightness, COLORCORR_KEY_UNIT ) );
		WRITE_SHORT( KeyShort( m_flGamma, COLORCORR_KEY_UNIT ) );
		WRITE_BYTE( m_iTint[0] );
		WRITE_BYTE( m_iTint[1] );
		WRITE_BYTE( m_iTint[2] );
		WRITE_BYTE( (int)( Clamp01( m_flTintAmount ) * 255.0f + 0.5f ) );
		WRITE_STRING( FStringNull( m_iszFile ) ? "" : STRING( m_iszFile ) );
	MESSAGE_END();
}

// the players in the game (the others get every entity with their HUD: ColorCorr_SendAll)
void CColorCorrectionBase::SendToAll( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->m_fGameHUDInitialized && !pPlayer->has_disconnected )
			SendTo( pPlayer, FALSE );
	}
}

//---------------------------------------------------------
// color_correction: a point
//---------------------------------------------------------
class CColorCorrection : public CColorCorrectionBase
{
public:
	void	KeyValue( KeyValueData *pkvd );
	void	Spawn( void );
	BOOL	IsVolume( void ) { return FALSE; }
};

LINK_ENTITY_TO_CLASS( color_correction, CColorCorrection );

//---------------------------------------------------------
// color_correction_volume: a brush
//---------------------------------------------------------
class CColorCorrectionVolume : public CColorCorrectionBase
{
public:
	void	KeyValue( KeyValueData *pkvd );
	void	Spawn( void );
	BOOL	IsVolume( void ) { return TRUE; }
};

LINK_ENTITY_TO_CLASS( color_correction_volume, CColorCorrectionVolume );

void CColorCorrection::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "fadeInDuration" ) )
	{
		m_flFadeIn = ClampTime( atof( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fadeOutDuration" ) )
	{
		m_flFadeOut = ClampTime( atof( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "minfalloff" ) )
	{
		m_flMinFalloff = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "maxfalloff" ) )
	{
		m_flMaxFalloff = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CColorCorrectionBase::KeyValue( pkvd );
}

void CColorCorrection::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
	SpawnCommon();
}

void CColorCorrectionVolume::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "fadeDuration" ) )
	{
		m_flFadeDuration = ClampTime( atof( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else
		CColorCorrectionBase::KeyValue( pkvd );
}

void CColorCorrectionVolume::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	if ( FStringNull( pev->model ) )
	{
		ALERT( at_error, "color_correction_volume \"%s\" without a brush at (%g %g %g)\n", STRING( pev->targetname ),
			pev->origin.x, pev->origin.y, pev->origin.z );
		pev->effects |= EF_NODRAW;
		m_bUsable = FALSE;
		m_bSpawned = TRUE;
		return;
	}
	SET_MODEL( ENT( pev ), STRING( pev->model ) );	// the brush model's index for the clients
	pev->effects |= EF_NODRAW;
	SpawnCommon();
}

//---------------------------------------------------------
void ColorCorr_SendAll( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	BOOL bFirst = TRUE;
	static const char *const s_pszClasses[] = { "color_correction", "color_correction_volume" };
	for ( const char *pszClass : s_pszClasses )
	{
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, pszClass ) ) != NULL )
		{
			CColorCorrectionBase *pCorr = (CColorCorrectionBase *)pEntity;
			if ( !pCorr->Usable() )
				continue;
			pCorr->SendTo( pPlayer, bFirst );
			bFirst = FALSE;
		}
	}
}
