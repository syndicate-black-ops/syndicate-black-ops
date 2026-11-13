//=========================================================
// Aliases: info_alias, multi_alias, trigger_changealias, info_group (docs/ALIASES.md)
//
// Ported from Spirit of Half-Life 1.9 (SpiritSource/dlls/alias.cpp, util.cpp UTIL_FollowReference; Laurie Cheers,
// Andrew J Hamilton; Half-Life 1 SDK license). SBO: the references are resolved in the one place every targetname
// search goes through (UTIL_FindEdictByString, util.cpp), so "*alias" and "group.member" work in Target, Kill target,
// Master and every other key that names entities, next to the selectors (!activator, !caller) and wildcards
// ("name*"), which stay as they were.
//
//   "*name"            the entities the alias(es) named "name" point to now. "*locus" and "*player" keep their meaning
//                      (an alias can't be named "locus" / "player", nor anything starting with '*'); "*" alone stays
//                      the wildcard that matches every name; "*name*" is refused (nothing found, a console line).
//   "group.member"     the entities the member of the info_group(s) named "group" names ("*alias.member": the group
//                      the alias points to; "group.a.b": member b of the group that member a names). Only when an
//                      info_group named "group" (or the alias) exists: other names with a dot are plain names.
//
// SBO changes to Spirit: an alias can point to an entity, not only to a name (trigger_changealias "!activator" /
// "*locus", or "Resolve references": Spirit stored that entity's name, so a player without a name pointed nowhere);
// info_alias's list mode switches to the new entry at once (Spirit's alias lagged one trigger behind) and Off steps
// back without leaving the list; multi_alias's random choice uses the engine's random numbers, one choice per game
// moment (Spirit reseeded the C library's rand()); references that loop stop at 16 levels with a console note.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"alias.h"

#define SF_ALIAS_OFF			1
#define SF_ALIAS_DEBUG			2
#define SF_GROUP_DEBUG			2
#define SF_CHANGEALIAS_RESOLVE	1
#define SF_CHANGEALIAS_DEBUG	2

#define MAX_ALIAS_TARGETS		16		// info_alias list mode (Spirit)
#define ALIAS_MAX_DEPTH			16
#define ALIAS_MAX_PENDING		128

static int s_iAliasDepth;
static int s_iFireDepth;
static EHANDLE s_hPending[ ALIAS_MAX_PENDING ];
static int s_cPending;

static BOOL IsAliasClass( entvars_t *pev )
{
	return FClassnameIs( pev, "info_alias" ) || FClassnameIs( pev, "multi_alias" );
}

static int EdictIndex( edict_t *pEdict )
{
	return FNullEnt( pEdict ) ? 0 : ENTINDEX( pEdict );
}

// the earlier of two results of a search after the same start (NULL: none)
static edict_t *EarlierEdict( edict_t *a, edict_t *b )
{
	if ( FNullEnt( a ) )
		return FNullEnt( b ) ? NULL : b;
	if ( FNullEnt( b ) )
		return a;
	return ENTINDEX( b ) < ENTINDEX( a ) ? b : a;
}

// An alias or group whose own name starts with '*', or an alias named "locus" / "player" (reached as "*locus" /
// "*player", which are built-in names), could never be reached: it is refused with a console line and removed
static BOOL Alias_RefuseName( CBaseEntity *pEntity, BOOL fAlias )
{
	const char *szName = STRING( pEntity->pev->targetname );
	const char *szWhy = NULL;
	if ( szName[0] == '*' )
		szWhy = "a name starts without the *; you write the * only where you use it";
	else if ( fAlias && ( FStrEq( szName, "locus" ) || FStrEq( szName, "player" ) ) )
		szWhy = "*locus and *player are built-in names";
	if ( !szWhy )
		return FALSE;
	ALERT( at_console, "%s \"%s\": refused (%s)\n", STRING( pEntity->pev->classname ), szName, szWhy );
	UTIL_Remove( pEntity );
	return TRUE;
}

//=========================================================
// CBaseAlias: what an alias points to now (a name or an entity), and the change that waits for the end of the firing
//=========================================================

class CBaseAlias : public CPointEntity
{
public:
	int		ObjectCaps( void ) { return CPointEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	// the next entity after entStart this alias points to
	virtual edict_t *FollowAlias( edict_t *entStart );
	virtual BOOL	CanChange( void ) { return TRUE; }

	void	ChangeValue( string_t iszName, CBaseEntity *pEntity );	// pEntity set: the alias points to that entity
	void	FlushChange( void );
	void	SetValue( string_t iszName, CBaseEntity *pEntity );
	virtual BOOL	Debug( void ) { return FALSE; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t	m_iszValue;			// the name it points to (or the name of its entity, for the console)
	EHANDLE		m_hValue;			// the entity it points to, when m_fEntityValue
	BOOL		m_fEntityValue;
	string_t	m_iszPending;		// a change made while targets were being fired
	EHANDLE		m_hPending;
	BOOL		m_fPendingEntity;
	BOOL		m_fPending;
};

TYPEDESCRIPTION CBaseAlias::m_SaveData[] =
{
	DEFINE_FIELD( CBaseAlias, m_iszValue, FIELD_STRING ),
	DEFINE_FIELD( CBaseAlias, m_hValue, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseAlias, m_fEntityValue, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBaseAlias, m_iszPending, FIELD_STRING ),
	DEFINE_FIELD( CBaseAlias, m_hPending, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseAlias, m_fPendingEntity, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBaseAlias, m_fPending, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CBaseAlias, CPointEntity );

edict_t *CBaseAlias::FollowAlias( edict_t *entStart )
{
	if ( m_fEntityValue )
	{
		CBaseEntity *pEntity = m_hValue;
		if ( pEntity && EdictIndex( pEntity->edict() ) > EdictIndex( entStart ) )
			return pEntity->edict();
		return NULL;
	}
	if ( FStringNull( m_iszValue ) || !STRING( m_iszValue )[0] )
		return NULL;
	return UTIL_FindEdictByString( entStart, "targetname", STRING( m_iszValue ) );
}

void CBaseAlias::SetValue( string_t iszName, CBaseEntity *pEntity )
{
	m_fEntityValue = pEntity != NULL;
	m_hValue = pEntity;
	m_iszValue = pEntity ? pEntity->pev->targetname : iszName;
	if ( Debug() )
	{
		if ( pEntity )
			ALERT( at_console, "DEBUG: %s \"%s\" now points to %s #%d\n", STRING( pev->classname ), STRING( pev->targetname ),
				STRING( pEntity->pev->classname ), ENTINDEX( pEntity->edict() ) );
		else
			ALERT( at_console, "DEBUG: %s \"%s\" now points to \"%s\"\n", STRING( pev->classname ), STRING( pev->targetname ),
				STRING( iszName ) );
	}
}

// Spirit: an alias that changes while targets are being fired would change the search under way; the change waits
// until the outermost FireTargets is done (Alias_FireTargetsDone)
void CBaseAlias::ChangeValue( string_t iszName, CBaseEntity *pEntity )
{
	if ( s_iFireDepth <= 0 )
	{
		SetValue( iszName, pEntity );
		return;
	}
	m_iszPending = iszName;
	m_hPending = pEntity;
	m_fPendingEntity = pEntity != NULL;
	if ( !m_fPending )
	{
		m_fPending = TRUE;
		if ( s_cPending < ALIAS_MAX_PENDING )
			s_hPending[ s_cPending++ ] = this;
		else
			FlushChange();	// more changes in one firing than the list holds: this one now
	}
}

void CBaseAlias::FlushChange( void )
{
	if ( !m_fPending )
		return;
	m_fPending = FALSE;
	CBaseEntity *pEntity = m_hPending;
	if ( m_fPendingEntity && !pEntity )
		SetValue( iStringNull, NULL );	// the entity went away meanwhile: it points nowhere
	else
		SetValue( m_iszPending, m_fPendingEntity ? pEntity : NULL );
}

//=========================================================
// info_alias: On points to "target", Off to "netname" (mode 0); or steps through a list (mode 1)
//=========================================================

class CInfoAlias : public CBaseAlias
{
public:
	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	BOOL	Debug( void ) { return FBitSet( pev->spawnflags, SF_ALIAS_DEBUG ) != 0; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int			m_iMode;			// 0 On / Off, 1 list
	int			m_cTargets;
	string_t	m_iTargetName[ MAX_ALIAS_TARGETS ];
	int			m_iCurrentTarget;
};

LINK_ENTITY_TO_CLASS( info_alias, CInfoAlias );

TYPEDESCRIPTION CInfoAlias::m_SaveData[] =
{
	DEFINE_FIELD( CInfoAlias, m_iMode, FIELD_INTEGER ),
	DEFINE_FIELD( CInfoAlias, m_cTargets, FIELD_INTEGER ),
	DEFINE_ARRAY( CInfoAlias, m_iTargetName, FIELD_STRING, MAX_ALIAS_TARGETS ),
	DEFINE_FIELD( CInfoAlias, m_iCurrentTarget, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CInfoAlias, CBaseAlias );

void CInfoAlias::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "mode" ) )
	{
		m_iMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
		return;
	}

	// list mode: every other key is a name, its value its place in the list (1-16; a "#n" suffix makes duplicates)
	pkvd->fHandled = TRUE;
	char tmp[128];
	UTIL_StripToken( pkvd->szKeyName, tmp, sizeof( tmp ) );
	const int iOrder = atoi( pkvd->szValue );
	if ( iOrder < 1 || iOrder > MAX_ALIAS_TARGETS )
		ALERT( at_console, "info_alias \"%s\": \"%s\" has the place %d (1-%d), left out\n", STRING( pev->targetname ), tmp,
			iOrder, MAX_ALIAS_TARGETS );
	else if ( m_iTargetName[ iOrder - 1 ] )
		ALERT( at_console, "info_alias \"%s\": \"%s\" has place %d, which \"%s\" has already; left out\n",
			STRING( pev->targetname ), tmp, iOrder, STRING( m_iTargetName[ iOrder - 1 ] ) );
	else
		m_iTargetName[ iOrder - 1 ] = ALLOC_STRING( tmp );
}

void CInfoAlias::Spawn( void )
{
	CPointEntity::Spawn();
	if ( Alias_RefuseName( this, TRUE ) )
		return;

	// the list in its order, gaps closed
	m_cTargets = 0;
	for ( int i = 0; i < MAX_ALIAS_TARGETS; i++ )
	{
		if ( m_iTargetName[i] )
			m_iTargetName[ m_cTargets++ ] = m_iTargetName[i];
	}
	for ( int i = m_cTargets; i < MAX_ALIAS_TARGETS; i++ )
		m_iTargetName[i] = iStringNull;

	m_iCurrentTarget = 0;
	if ( m_iMode == 1 )
		SetValue( m_cTargets ? m_iTargetName[0] : iStringNull, NULL );
	else
		SetValue( FBitSet( pev->spawnflags, SF_ALIAS_OFF ) ? pev->netname : pev->target, NULL );
}

void CInfoAlias::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_iMode == 1 )
	{
		if ( !m_cTargets )
			return;
		// a trigger_changealias may have pointed it at one of the list's names: the step starts from that one
		if ( !m_fEntityValue && !FStringNull( m_iszValue ) )
		{
			for ( int i = 0; i < m_cTargets; i++ )
				if ( FStrEq( STRING( m_iszValue ), STRING( m_iTargetName[i] ) ) )
					m_iCurrentTarget = i;
		}
		// On / Toggle: the next entry, Off: the one before (the list wraps around)
		if ( useType == USE_OFF )
			m_iCurrentTarget = ( m_iCurrentTarget + m_cTargets - 1 ) % m_cTargets;
		else
			m_iCurrentTarget = ( m_iCurrentTarget + 1 ) % m_cTargets;
		ChangeValue( m_iTargetName[ m_iCurrentTarget ], NULL );
		return;
	}

	const BOOL fOn = !FBitSet( pev->spawnflags, SF_ALIAS_OFF );
	if ( !ShouldToggle( useType, fOn ) )
		return;
	if ( fOn )
		SetBits( pev->spawnflags, SF_ALIAS_OFF );
	else
		ClearBits( pev->spawnflags, SF_ALIAS_OFF );
	ChangeValue( fOn ? pev->netname : pev->target, NULL );
}

//=========================================================
// multi_alias: points to several names at once, or to a random choice of them
//=========================================================

#define MULTIALIAS_ALL		0
#define MULTIALIAS_CHOOSE	1
#define MULTIALIAS_PERCENT	2

class CMultiAlias : public CBaseAlias
{
public:
	void	Spawn( void ) { CPointEntity::Spawn(); Alias_RefuseName( this, TRUE ); }
	void	KeyValue( KeyValueData *pkvd );
	edict_t	*FollowAlias( edict_t *entStart );
	BOOL	CanChange( void ) { return FALSE; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void	Choose( void );

	int			m_iMode;
	int			m_cTargets;
	string_t	m_iszTargets[ MAX_MULTI_TARGETS ];
	int			m_iValues[ MAX_MULTI_TARGETS ];
	int			m_iTotalValue;
	float		m_flChoiceTime;		// the game moment of the last random choice (not saved: chosen again after a load)
	BOOL		m_fChosen[ MAX_MULTI_TARGETS ];
};

LINK_ENTITY_TO_CLASS( multi_alias, CMultiAlias );

TYPEDESCRIPTION CMultiAlias::m_SaveData[] =
{
	DEFINE_FIELD( CMultiAlias, m_iMode, FIELD_INTEGER ),
	DEFINE_FIELD( CMultiAlias, m_cTargets, FIELD_INTEGER ),
	DEFINE_ARRAY( CMultiAlias, m_iszTargets, FIELD_STRING, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY( CMultiAlias, m_iValues, FIELD_INTEGER, MAX_MULTI_TARGETS ),
	DEFINE_FIELD( CMultiAlias, m_iTotalValue, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CMultiAlias, CBaseAlias );

void CMultiAlias::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iMode" ) )
	{
		m_iMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
		return;
	}

	// every other key is a name, its value the name's weight (choose one) or percent chance
	pkvd->fHandled = TRUE;
	if ( m_cTargets >= MAX_MULTI_TARGETS )
	{
		ALERT( at_console, "multi_alias \"%s\": too many names (more than %d), \"%s\" left out\n", STRING( pev->targetname ),
			MAX_MULTI_TARGETS, pkvd->szKeyName );
		return;
	}
	char tmp[128];
	UTIL_StripToken( pkvd->szKeyName, tmp, sizeof( tmp ) );
	m_iszTargets[ m_cTargets ] = ALLOC_STRING( tmp );
	m_iValues[ m_cTargets ] = max( atoi( pkvd->szValue ), 0 );
	m_iTotalValue += m_iValues[ m_cTargets ];
	m_cTargets++;
}

// one random choice per game moment, so every search of one firing sees the same entities (Spirit)
void CMultiAlias::Choose( void )
{
	if ( m_flChoiceTime == gpGlobals->time && m_flChoiceTime != 0 )
		return;
	m_flChoiceTime = gpGlobals->time;

	for ( int i = 0; i < m_cTargets; i++ )
		m_fChosen[i] = FALSE;

	if ( m_iMode == MULTIALIAS_CHOOSE )
	{
		if ( m_iTotalValue <= 0 )
			return;
		int iRandom = RANDOM_LONG( 1, m_iTotalValue );
		for ( int i = 0; i < m_cTargets; i++ )
		{
			iRandom -= m_iValues[i];
			if ( iRandom <= 0 )
			{
				m_fChosen[i] = TRUE;
				break;
			}
		}
	}
	else
	{
		for ( int i = 0; i < m_cTargets; i++ )
			m_fChosen[i] = m_iValues[i] > RANDOM_LONG( 0, 99 );
	}
}

edict_t *CMultiAlias::FollowAlias( edict_t *entStart )
{
	if ( m_iMode == MULTIALIAS_CHOOSE || m_iMode == MULTIALIAS_PERCENT )
		Choose();

	edict_t *pBest = NULL;
	for ( int i = 0; i < m_cTargets; i++ )
	{
		if ( m_iMode != MULTIALIAS_ALL && !m_fChosen[i] )
			continue;
		pBest = EarlierEdict( pBest, UTIL_FindEdictByString( entStart, "targetname", STRING( m_iszTargets[i] ) ) );
	}
	return pBest;
}

//=========================================================
// trigger_changealias: points an alias somewhere else
//=========================================================

class CTriggerChangeAlias : public CPointEntity
{
public:
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int		ObjectCaps( void ) { return CPointEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

LINK_ENTITY_TO_CLASS( trigger_changealias, CTriggerChangeAlias );

void CTriggerChangeAlias::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	const BOOL fDebug = FBitSet( pev->spawnflags, SF_CHANGEALIAS_DEBUG ) != 0;
	BOOL fChanged = FALSE;

	// every alias with that name (a plain name finds the alias itself)
	CBaseEntity *pTarget = NULL;
	while ( !FStringNull( pev->target ) && ( pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ), pActivator, this ) ) != NULL )
	{
		if ( !IsAliasClass( pTarget->pev ) || !( (CBaseAlias *)pTarget )->CanChange() )
			continue;

		CBaseEntity *pValue = NULL;
		const char *szValue = STRING( pev->netname );
		if ( FStrEq( szValue, "*locus" ) || FStrEq( szValue, "!activator" ) )
			pValue = pActivator;
		else if ( FStrEq( szValue, "!caller" ) )
			pValue = pCaller;
		else if ( FBitSet( pev->spawnflags, SF_CHANGEALIAS_RESOLVE ) && szValue[0] )
			pValue = UTIL_FindEntityByTargetname( NULL, szValue, pActivator, pCaller );

		if ( pValue )
			( (CBaseAlias *)pTarget )->ChangeValue( iStringNull, pValue );
		else if ( FBitSet( pev->spawnflags, SF_CHANGEALIAS_RESOLVE ) || FStrEq( szValue, "*locus" ) || szValue[0] == '!' )
			( (CBaseAlias *)pTarget )->ChangeValue( iStringNull, NULL );	// that entity doesn't exist: nowhere
		else
			( (CBaseAlias *)pTarget )->ChangeValue( pev->netname, NULL );
		fChanged = TRUE;

		if ( fDebug )
			ALERT( at_console, "DEBUG: trigger_changealias \"%s\" points %s \"%s\" to \"%s\"\n", STRING( pev->targetname ),
				STRING( pTarget->pev->classname ), STRING( pTarget->pev->targetname ), szValue );
		if ( pTarget == pActivator || pTarget == pCaller )
			break;	// a selector names one entity
	}

	if ( !fChanged )
		ALERT( at_console, "trigger_changealias \"%s\": no info_alias named \"%s\"\n", STRING( pev->targetname ), STRING( pev->target ) );
}

//=========================================================
// info_group: a set of names under one name ("group.member"); triggered, points its alias to itself
//=========================================================

class CInfoGroup : public CPointEntity
{
public:
	void	Spawn( void ) { CPointEntity::Spawn(); Alias_RefuseName( this, FALSE ); }
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int		ObjectCaps( void ) { return CPointEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	BOOL	GetMember( const char *szMember, char *szOut, int iOutSize );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int			m_cMembers;
	string_t	m_iszMemberName[ MAX_MULTI_TARGETS ];
	string_t	m_iszMemberValue[ MAX_MULTI_TARGETS ];
	string_t	m_iszDefaultMember;
};

LINK_ENTITY_TO_CLASS( info_group, CInfoGroup );

TYPEDESCRIPTION CInfoGroup::m_SaveData[] =
{
	DEFINE_FIELD( CInfoGroup, m_cMembers, FIELD_INTEGER ),
	DEFINE_ARRAY( CInfoGroup, m_iszMemberName, FIELD_STRING, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY( CInfoGroup, m_iszMemberValue, FIELD_STRING, MAX_MULTI_TARGETS ),
	DEFINE_FIELD( CInfoGroup, m_iszDefaultMember, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CInfoGroup, CPointEntity );

void CInfoGroup::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "defaultmember" ) )
	{
		m_iszDefaultMember = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		return;
	}

	// every other key is a member: its name, and the entity name it stands for
	pkvd->fHandled = TRUE;
	if ( m_cMembers >= MAX_MULTI_TARGETS )
	{
		ALERT( at_console, "info_group \"%s\": too many members (more than %d), \"%s\" left out\n", STRING( pev->targetname ),
			MAX_MULTI_TARGETS, pkvd->szKeyName );
		return;
	}
	char tmp[128];
	UTIL_StripToken( pkvd->szKeyName, tmp, sizeof( tmp ) );
	m_iszMemberName[ m_cMembers ] = ALLOC_STRING( tmp );
	m_iszMemberValue[ m_cMembers ] = ALLOC_STRING( pkvd->szValue );
	m_cMembers++;
}

// the name the member stands for; a member it lacks is "defaultmember" + the member's name (Spirit)
BOOL CInfoGroup::GetMember( const char *szMember, char *szOut, int iOutSize )
{
	for ( int i = 0; i < m_cMembers; i++ )
	{
		if ( FStrEq( szMember, STRING( m_iszMemberName[i] ) ) )
		{
			snprintf( szOut, iOutSize, "%s", STRING( m_iszMemberValue[i] ) );
			return TRUE;
		}
	}
	if ( m_iszDefaultMember )
	{
		snprintf( szOut, iOutSize, "%s%s", STRING( m_iszDefaultMember ), szMember );
		return TRUE;
	}
	if ( FBitSet( pev->spawnflags, SF_GROUP_DEBUG ) )
		ALERT( at_console, "DEBUG: info_group \"%s\" has no member \"%s\"\n", STRING( pev->targetname ), szMember );
	return FALSE;
}

void CInfoGroup::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	BOOL fChanged = FALSE;
	CBaseEntity *pTarget = NULL;
	while ( !FStringNull( pev->target ) && ( pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ), pActivator, this ) ) != NULL )
	{
		if ( !IsAliasClass( pTarget->pev ) || !( (CBaseAlias *)pTarget )->CanChange() )
			continue;
		( (CBaseAlias *)pTarget )->ChangeValue( pev->targetname, NULL );
		fChanged = TRUE;
		if ( FBitSet( pev->spawnflags, SF_GROUP_DEBUG ) )
			ALERT( at_console, "DEBUG: info_group \"%s\" points %s \"%s\" to itself\n", STRING( pev->targetname ),
				STRING( pTarget->pev->classname ), STRING( pTarget->pev->targetname ) );
		if ( pTarget == pActivator || pTarget == pCaller )
			break;
	}
	if ( !fChanged && !FStringNull( pev->target ) )
		ALERT( at_console, "info_group \"%s\": no info_alias named \"%s\"\n", STRING( pev->targetname ), STRING( pev->target ) );
}

//=========================================================
// The references (util.cpp UTIL_FindEdictByString)
//=========================================================

static BOOL Alias_Enter( const char *szName )
{
	if ( s_iAliasDepth >= ALIAS_MAX_DEPTH )
	{
		if ( s_iAliasDepth == ALIAS_MAX_DEPTH )
			ALERT( at_console, "\"%s\": aliases or groups that point to each other in a loop; nothing found\n", szName );
		return FALSE;
	}
	s_iAliasDepth++;
	return TRUE;
}

// "*name": the earliest entity after entStart that any alias named "name" points to
static edict_t *Alias_FollowAlias( edict_t *entStart, const char *szAlias )
{
	edict_t *pBest = NULL;
	edict_t *pEdict = NULL;
	while ( !FNullEnt( pEdict = FIND_ENTITY_BY_STRING( pEdict, "targetname", szAlias ) ) )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
		if ( pEntity && IsAliasClass( pEntity->pev ) && !( pEntity->pev->flags & FL_KILLME ) )
			pBest = EarlierEdict( pBest, ( (CBaseAlias *)pEntity )->FollowAlias( entStart ) );
	}
	return pBest;
}

// "group.member[.more]": the earliest entity after entStart that the member names, over every info_group the group
// name finds (itself a name, "*alias" or a group reference)
static edict_t *Alias_FollowGroup( edict_t *entStart, const char *szGroup, const char *szMember )
{
	char szThis[128], szName[256];
	const char *szTail = strchr( szMember, '.' );
	if ( szTail )
	{
		snprintf( szThis, sizeof( szThis ), "%.*s", (int)( szTail - szMember ), szMember );
		szTail++;
	}
	else
		snprintf( szThis, sizeof( szThis ), "%s", szMember );

	edict_t *pBest = NULL;
	edict_t *pEdict = NULL;
	while ( !FNullEnt( pEdict = UTIL_FindEdictByString( pEdict, "targetname", szGroup ) ) )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
		if ( !pEntity || !FClassnameIs( pEntity->pev, "info_group" ) )
			continue;
		char szValue[128];
		if ( !( (CInfoGroup *)pEntity )->GetMember( szThis, szValue, sizeof( szValue ) ) || !szValue[0] )
			continue;
		if ( szTail )
			snprintf( szName, sizeof( szName ), "%s.%s", szValue, szTail );
		else
			snprintf( szName, sizeof( szName ), "%s", szValue );
		pBest = EarlierEdict( pBest, UTIL_FindEdictByString( entStart, "targetname", szName ) );
	}
	return pBest;
}

// is there an info_group with this name (or is it an alias reference, which may point to one)?
static BOOL Alias_IsGroupName( const char *szName )
{
	if ( szName[0] == '*' && szName[1] )
		return TRUE;
	edict_t *pEdict = NULL;
	while ( !FNullEnt( pEdict = FIND_ENTITY_BY_STRING( pEdict, "targetname", szName ) ) )
	{
		if ( FClassnameIs( &pEdict->v, "info_group" ) )
			return TRUE;
	}
	return FALSE;
}

BOOL Alias_FindEdict( edict_t *entStart, const char *szName, edict_t **ppResult )
{
	*ppResult = NULL;
	if ( !szName || !szName[0] )
		return FALSE;

	// a group reference: "group.member" where "group" is an info_group (or "*alias"); other dotted names are names
	const char *szDot = strchr( szName, '.' );
	if ( szDot && szDot != szName && szDot[1] )
	{
		char szGroup[128];
		snprintf( szGroup, sizeof( szGroup ), "%.*s", (int)( szDot - szName ), szName );
		if ( Alias_IsGroupName( szGroup ) )
		{
			if ( Alias_Enter( szName ) )
			{
				*ppResult = Alias_FollowGroup( entStart, szGroup, szDot + 1 );
				s_iAliasDepth--;
			}
			return TRUE;
		}
	}

	// an alias reference: "*name" (not "*locus" / "*player", which the Locus code answers, nor "*" alone)
	if ( szName[0] == '*' && szName[1] && !FStrEq( szName, "*locus" ) && !FStrEq( szName, "*player" ) )
	{
		// "*name*": an alias reference and a wildcard at once is refused (point an alias at "name*" instead)
		if ( szName[ strlen( szName ) - 1 ] == '*' )
		{
			ALERT( at_console, "\"%s\": a * at both ends is not allowed, nothing found (an alias can point to a wildcard)\n",
				szName );
			return TRUE;
		}
		if ( Alias_Enter( szName ) )
		{
			*ppResult = Alias_FollowAlias( entStart, szName + 1 );
			s_iAliasDepth--;
		}
		return TRUE;
	}

	return FALSE;
}

// FireTargets: alias changes made while it fires wait until the outermost one is done (Spirit's UTIL_FlushAliases)
void Alias_FireTargetsBegin( void )
{
	s_iFireDepth++;
}

void Alias_FireTargetsDone( void )
{
	if ( --s_iFireDepth > 0 )
		return;
	s_iFireDepth = 0;
	for ( int i = 0; i < s_cPending; i++ )
	{
		CBaseAlias *pAlias = (CBaseAlias *)(CBaseEntity *)s_hPending[i];
		s_hPending[i] = NULL;
		if ( pAlias )
			pAlias->FlushChange();
	}
	s_cPending = 0;
}

// a new map: nothing pending from the last one
void Alias_Reset( void )
{
	s_iFireDepth = 0;
	s_iAliasDepth = 0;
	for ( int i = 0; i < s_cPending; i++ )
		s_hPending[i] = NULL;
	s_cPending = 0;
}
