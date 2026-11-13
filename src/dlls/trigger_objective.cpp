//=========================================================
// SBO 2.0 trigger_objective (docs/OBJECTIVES.md): a mission objective every player sees in the HUD's objective list,
// the scoreboard and the map briefing.
//
//   * States: Hidden (not shown), Active, Completed. A trigger moves a Hidden objective to Active and an Active one to
//     Completed; with a counter ("count" N) every trigger counts one up ("1/3"; the first one also shows a hidden
//     objective) and it is completed at N. A Completed objective ignores triggers. "target" is fired when it is completed (the completing trigger's
//     activator). "master": triggers are ignored while the master is off.
//   * "state" 1 / 2: Active / Completed at map start. "order": the list position (0 = after the numbered ones, in the
//     order they became active).
//   * Groups: "partof" names another trigger_objective, the group; the part is listed under it. A group shows how
//     many of its parts are done ("1/3"; its own "count" is not used) and is completed when all of them are, firing
//     its target; a trigger shows a hidden group and does nothing more. A group shown shows its hidden parts with it,
//     and a part that becomes active shows its hidden group (one notice, the group's). One level: a group can't be a part (a console line; the part is then on
//     its own).
//   * Flags: 1 Silent (no "NEW OBJECTIVE" notice, no sound), 2 Reset on new round (each round start puts it back to
//     its map-start state; without it an objective keeps its state over co-op round restarts).
//   * Everyone sees the same list (the team's mission): every change goes to all clients (MSG_ALL, HLTV included);
//     a client that (re)initializes its HUD (joining, spectating, fullupdate, a demo started mid-map) gets the whole
//     list through the send queue (client_sendqueue.h, SENDQUEUE_OBJECTIVE).
//   * At most OBJECTIVE_MAX per map (more are removed at map start, with a console line); the text is cut at
//     OBJECTIVE_MAX_TEXT - 1 bytes (a console line).
// Message format: game_shared/objective_shared.h.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"saverestore.h"
#include	"client_sendqueue.h"
#include	"objective_shared.h"

extern int gmsgObjective;	// player.cpp

#define SF_OBJECTIVE_SILENT			1
#define SF_OBJECTIVE_ROUND_RESET	2

// the send queue (SENDQUEUE_OBJECTIVE): after the HUD initialization, a round of messages every 0.1 s
#define OBJECTIVE_QUEUE_DELAY		0.2f
#define OBJECTIVE_QUEUE_INTERVAL	0.1f
#define OBJECTIVE_QUEUE_BYTES		600

class CTriggerObjective : public CPointEntity
{
public:
	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	RoundReset( void );
	void	Activate( void );	// after the map's spawn: the group named by "partof"
	int		ObjectCaps( void ) { return CPointEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int		Slot( void ) { return m_iSlot - 1; }	// -1: none (not spawned yet, or over the limit)
	// the message for one client (pPlayer) or all (NULL); iEvents: OBJECTIVE_EV_* of a change happening now
	void	Send( CBasePlayer *pPlayer, int iEvents );
	int		MessageBytes( void );

	int		m_iSlot;		// slot + 1 (0 = none)
	int		m_iStartState;	// "state"
	int		m_iState;		// OBJECTIVE_HIDDEN / _ACTIVE / _COMPLETED
	int		m_iOrder;		// "order"
	int		m_iTarget;		// "count": the counter's target (0 = no counter)
	int		m_iCount;
	int		m_iSequence;	// when it became active (0 = never)
	string_t	m_sMaster;
	string_t	m_iszPartOf;	// "partof": the group's name
	int		m_iParent;		// the group's slot + 1 (0 = none; Activate)

	CTriggerObjective *Group( void );
	BOOL	IsGroup( void );	// some objective is a part of this one
	void	CountParts( int &iDone, int &iTotal );
	// a part changed: the group's count goes to the clients; all parts done: the group is completed
	void	PartChanged( CBaseEntity *pActivator, BOOL bShowGroup );

private:
	void	SetStartState( void );
	void	Show( void );	// Hidden -> Active
	void	ShowParts( void );	// a group shown: its hidden parts with it (sent, no notice of their own)
	void	Complete( CBaseEntity *pActivator, int iEvents );
};

LINK_ENTITY_TO_CLASS( trigger_objective, CTriggerObjective );

TYPEDESCRIPTION CTriggerObjective::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerObjective, m_iSlot, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerObjective, m_iStartState, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerObjective, m_iState, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerObjective, m_iOrder, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerObjective, m_iTarget, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerObjective, m_iCount, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerObjective, m_iSequence, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerObjective, m_sMaster, FIELD_STRING ),
	DEFINE_FIELD( CTriggerObjective, m_iszPartOf, FIELD_STRING ),
	DEFINE_FIELD( CTriggerObjective, m_iParent, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CTriggerObjective, CPointEntity );

static const char *Objective_Name( CBaseEntity *pEntity )
{
	return pEntity->pev->targetname ? STRING( pEntity->pev->targetname ) : "";
}

// every trigger_objective of the map with a slot, by slot
static CTriggerObjective *Objective_BySlot( int iSlot )
{
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "trigger_objective" ) ) != NULL )
	{
		CTriggerObjective *pObjective = (CTriggerObjective *)pEntity;
		if ( pObjective->Slot() == iSlot )
			return pObjective;
	}
	return NULL;
}

// the next activation number: one more than the newest one of the map
static int Objective_NextSequence( void )
{
	int iMax = 0;
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "trigger_objective" ) ) != NULL )
		iMax = max( iMax, ( (CTriggerObjective *)pEntity )->m_iSequence );
	return min( iMax + 1, 32767 );
}

void CTriggerObjective::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "state" ) )
	{
		m_iStartState = atoi( pkvd->szValue );
		if ( m_iStartState < OBJECTIVE_HIDDEN || m_iStartState > OBJECTIVE_COMPLETED )
			m_iStartState = OBJECTIVE_HIDDEN;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "order" ) )
	{
		m_iOrder = max( 0, min( atoi( pkvd->szValue ), OBJECTIVE_MAX_ORDER ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "count" ) )
	{
		m_iTarget = max( 0, min( atoi( pkvd->szValue ), OBJECTIVE_MAX_COUNT ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "master" ) )
	{
		m_sMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "partof" ) )
	{
		m_iszPartOf = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : 0;
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CTriggerObjective::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;

	// the lowest slot no other objective has (entities spawn in the map's order: the slot is the map's order)
	BOOL bUsed[ OBJECTIVE_MAX ] = {};
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "trigger_objective" ) ) != NULL )
	{
		const int iSlot = ( (CTriggerObjective *)pEntity )->Slot();
		if ( pEntity != this && iSlot >= 0 && iSlot < OBJECTIVE_MAX )
			bUsed[iSlot] = TRUE;
	}
	m_iSlot = 0;
	for ( int i = 0; i < OBJECTIVE_MAX; i++ )
	{
		if ( !bUsed[i] )
		{
			m_iSlot = i + 1;
			break;
		}
	}
	if ( !m_iSlot )
	{
		// printed on every server (not only with developer), as game_playvideo's errors
		g_engfuncs.pfnServerPrint( UTIL_VarArgs( "Error: trigger_objective \"%s\": more than %d objectives in the map, "
			"this one is removed\n", Objective_Name( this ), OBJECTIVE_MAX ) );
		UTIL_Remove( this );
		return;
	}

	if ( pev->message && strlen( STRING( pev->message ) ) > OBJECTIVE_MAX_TEXT - 1 )
		g_engfuncs.pfnServerPrint( UTIL_VarArgs( "trigger_objective \"%s\": the text is longer than %d characters and "
			"is cut\n", Objective_Name( this ), OBJECTIVE_MAX_TEXT - 1 ) );

	SetStartState();
}

void CTriggerObjective::SetStartState( void )
{
	m_iState = m_iStartState;
	m_iCount = m_iState == OBJECTIVE_COMPLETED ? m_iTarget : 0;
	m_iSequence = m_iState == OBJECTIVE_ACTIVE ? Objective_NextSequence() : 0;
}

void CTriggerObjective::Show( void )
{
	m_iState = OBJECTIVE_ACTIVE;
	m_iCount = 0;
	m_iSequence = Objective_NextSequence();
}

// after the map's spawn (ServerActivate): the group "partof" names
void CTriggerObjective::Activate( void )
{
	m_iParent = 0;
	if ( !m_iszPartOf || Slot() < 0 )
		return;
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( m_iszPartOf ) ) ) != NULL )
	{
		if ( !FClassnameIs( pEntity->pev, "trigger_objective" ) || pEntity == this )
			continue;
		CTriggerObjective *pGroup = (CTriggerObjective *)pEntity;
		if ( pGroup->Slot() < 0 )
			continue;
		if ( pGroup->m_iszPartOf )
		{
			g_engfuncs.pfnServerPrint( UTIL_VarArgs( "trigger_objective \"%s\": its group \"%s\" is a part itself (one "
				"level only): listed on its own\n", Objective_Name( this ), STRING( m_iszPartOf ) ) );
			return;
		}
		m_iParent = pGroup->Slot() + 1;
		// a part active from the map start shows its group; a group active from the map start shows its parts
		if ( m_iState == OBJECTIVE_ACTIVE && pGroup->m_iState == OBJECTIVE_HIDDEN )
			pGroup->Show();
		if ( m_iState == OBJECTIVE_HIDDEN && pGroup->m_iState == OBJECTIVE_ACTIVE )
			Show();
		return;
	}
	g_engfuncs.pfnServerPrint( UTIL_VarArgs( "trigger_objective \"%s\": no trigger_objective \"%s\" (partof): listed "
		"on its own\n", Objective_Name( this ), STRING( m_iszPartOf ) ) );
}

CTriggerObjective *CTriggerObjective::Group( void )
{
	return m_iParent ? Objective_BySlot( m_iParent - 1 ) : NULL;
}

BOOL CTriggerObjective::IsGroup( void )
{
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "trigger_objective" ) ) != NULL )
	{
		if ( ( (CTriggerObjective *)pEntity )->m_iParent == Slot() + 1 && Slot() >= 0 )
			return TRUE;
	}
	return FALSE;
}

void CTriggerObjective::CountParts( int &iDone, int &iTotal )
{
	iDone = iTotal = 0;
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "trigger_objective" ) ) != NULL )
	{
		CTriggerObjective *pPart = (CTriggerObjective *)pEntity;
		if ( Slot() < 0 || pPart->m_iParent != Slot() + 1 )
			continue;
		iTotal++;
		iDone += pPart->m_iState == OBJECTIVE_COMPLETED;
	}
}

void CTriggerObjective::ShowParts( void )
{
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "trigger_objective" ) ) != NULL )
	{
		CTriggerObjective *pPart = (CTriggerObjective *)pEntity;
		if ( Slot() < 0 || pPart->m_iParent != Slot() + 1 || pPart->m_iState != OBJECTIVE_HIDDEN )
			continue;
		pPart->Show();
		pPart->Send( NULL, 0 );
	}
}

void CTriggerObjective::Complete( CBaseEntity *pActivator, int iEvents )
{
	m_iState = OBJECTIVE_COMPLETED;
	Send( NULL, iEvents | OBJECTIVE_EV_DONE );
	// the target after the state is sent: a target that triggers this objective again finds it completed
	if ( pev->target )
		FireTargets( STRING( pev->target ), pActivator, this, USE_TOGGLE, 0 );
}

void CTriggerObjective::PartChanged( CBaseEntity *pActivator, BOOL bShowGroup )
{
	int iDone, iTotal;
	CountParts( iDone, iTotal );
	BOOL bShown = FALSE;
	if ( bShowGroup && m_iState == OBJECTIVE_HIDDEN )
	{
		Show();
		bShown = TRUE;
	}
	const int iEvents = bShown ? OBJECTIVE_EV_NEW : OBJECTIVE_EV_COUNT;
	if ( m_iState == OBJECTIVE_ACTIVE && iTotal > 0 && iDone >= iTotal )
		Complete( pActivator, iEvents );
	else if ( m_iState != OBJECTIVE_HIDDEN )
		Send( NULL, iEvents );
	if ( bShown )
		ShowParts();
}

void CTriggerObjective::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_sMaster && !UTIL_IsMasterTriggered( m_sMaster, pActivator ) )
		return;

	// a group: a trigger only shows it; its parts complete it
	if ( IsGroup() )
	{
		if ( m_iState != OBJECTIVE_HIDDEN )
			return;
		Show();
		Send( NULL, OBJECTIVE_EV_NEW );
		ShowParts();
		int iDone, iTotal;
		CountParts( iDone, iTotal );
		if ( iTotal > 0 && iDone >= iTotal )
			Complete( pActivator, 0 );
		return;
	}

	CTriggerObjective *pGroup = Group();
	int iEvents = 0;
	switch ( m_iState )
	{
	case OBJECTIVE_HIDDEN:
		Show();
		// a part that shows its hidden group: the notice is the group's
		iEvents = ( pGroup && pGroup->m_iState == OBJECTIVE_HIDDEN ) ? 0 : OBJECTIVE_EV_NEW;
		// a counter counts this trigger too (the first kill of "Kill 10 Feds" is 1/10)
		if ( m_iTarget > 0 )
		{
			m_iCount = 1;
			iEvents |= OBJECTIVE_EV_COUNT;
			if ( m_iCount >= m_iTarget )
			{
				Complete( pActivator, iEvents );
				if ( pGroup )
					pGroup->PartChanged( pActivator, TRUE );
				return;
			}
		}
		break;

	case OBJECTIVE_ACTIVE:
		if ( m_iTarget > 0 )
		{
			m_iCount++;
			iEvents = OBJECTIVE_EV_COUNT;
			if ( m_iCount < m_iTarget )
				break;
		}
		ALERT( at_aiconsole, "trigger_objective \"%s\": completed\n", Objective_Name( this ) );
		Complete( pActivator, iEvents );
		if ( pGroup )
			pGroup->PartChanged( pActivator, TRUE );
		return;

	default:
		return;	// completed: nothing more to do
	}

	ALERT( at_aiconsole, "trigger_objective \"%s\": active%s\n", Objective_Name( this ),
		m_iTarget > 0 ? UTIL_VarArgs( " %d/%d", m_iCount, m_iTarget ) : "" );

	// the group first (shown before its part)
	if ( pGroup && pGroup->m_iState == OBJECTIVE_HIDDEN )
		pGroup->PartChanged( pActivator, TRUE );
	Send( NULL, iEvents );
}

// SBO 2.0 round hooks (round_hooks.h): flag 2 puts it back to its map-start state at each round start
void CTriggerObjective::RoundReset( void )
{
	if ( !( pev->spawnflags & SF_OBJECTIVE_ROUND_RESET ) || Slot() < 0 )
		return;

	const int iOldState = m_iState, iOldCount = m_iCount;
	m_iSequence = 0;	// a fresh activation number when it starts Active
	SetStartState();
	if ( m_iState != iOldState || m_iCount != iOldCount || m_iState == OBJECTIVE_ACTIVE )
		Send( NULL, 0 );
	// a group's count of done parts changed
	CTriggerObjective *pGroup = Group();
	if ( pGroup && pGroup->m_iState != OBJECTIVE_HIDDEN && m_iState != iOldState )
		pGroup->Send( NULL, 0 );
}

int CTriggerObjective::MessageBytes( void )
{
	if ( m_iState == OBJECTIVE_HIDDEN )
		return 2;
	const int iText = pev->message ? min( (int)strlen( STRING( pev->message ) ), OBJECTIVE_MAX_TEXT - 1 ) : 0;
	return 9 + iText + 1;
}

void CTriggerObjective::Send( CBasePlayer *pPlayer, int iEvents )
{
	if ( !gmsgObjective || Slot() < 0 )
		return;

	int iFlags = m_iState | iEvents;
	if ( pev->spawnflags & SF_OBJECTIVE_SILENT )
		iFlags |= OBJECTIVE_SILENT;

	if ( pPlayer )
		MESSAGE_BEGIN( MSG_ONE, gmsgObjective, NULL, pPlayer->edict() );
	else
		MESSAGE_BEGIN( MSG_ALL, gmsgObjective );
		WRITE_BYTE( Slot() );
		WRITE_BYTE( iFlags );
		if ( m_iState != OBJECTIVE_HIDDEN )
		{
			char szText[ OBJECTIVE_MAX_TEXT ];
			strncpy( szText, pev->message ? STRING( pev->message ) : "", sizeof( szText ) - 1 );
			szText[ sizeof( szText ) - 1 ] = '\0';
			int iCount = m_iCount, iTarget = m_iTarget;
			if ( IsGroup() )
				CountParts( iCount, iTarget );	// a group counts its parts
			WRITE_SHORT( m_iOrder );
			WRITE_SHORT( m_iSequence );
			WRITE_BYTE( min( iCount, 255 ) );
			WRITE_BYTE( min( iTarget, 255 ) );
			WRITE_BYTE( m_iParent ? m_iParent - 1 : OBJECTIVE_NO_PARENT );
			WRITE_STRING( szText );
		}
	MESSAGE_END();
}

// the send queue (client_sendqueue.h, SENDQUEUE_OBJECTIVE): cursor 0 the clear, then cursor 1 + slot
static int Objective_SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget )
{
	if ( !gmsgObjective )
		return TRUE;

	if ( iCursor <= 0 )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgObjective, NULL, pPlayer->edict() );
			WRITE_BYTE( OBJECTIVE_SLOT_CLEAR );
		MESSAGE_END();
		iBudget -= 1;
		iCursor = 1;
	}

	for ( int iSlot = iCursor - 1; iSlot < OBJECTIVE_MAX; iSlot++ )
	{
		CTriggerObjective *pObjective = Objective_BySlot( iSlot );
		if ( !pObjective || pObjective->m_iState == OBJECTIVE_HIDDEN )
			continue;
		const int iBytes = pObjective->MessageBytes();
		if ( iBytes > iBudget )
		{
			iCursor = iSlot + 1;
			return FALSE;
		}
		pObjective->Send( pPlayer, 0 );
		iBudget -= iBytes;
	}
	return TRUE;
}

static SendQueue_Sender s_ObjectiveSender( SENDQUEUE_OBJECTIVE, Objective_SendQueued, 0, OBJECTIVE_QUEUE_DELAY,
	OBJECTIVE_QUEUE_INTERVAL, OBJECTIVE_QUEUE_BYTES );
