//=========================================================
// SBO 2.0 inventory: item_pickup, player_inventory, player_hasinventory. See inventory.h and docs/INVENTORY.md.
//
// Featureful's inventory is single player: 16 named slots per player, item_pickup a CItem that is gone once taken.
// SBO is multiplayer, so (SevenKewp item_inventory's model) an item_pickup is the item itself: taken, it is hidden
// and remembers who holds it (or that the shared pool does). Where it goes after that:
//   * its holder dies: it drops where he died and goes back to its placed spot after return_timelimit seconds
//     untouched, or at once when it falls out of reach (solid, sky, lava, slime); holder_keep_on_death 1 keeps it
//     with him through death and strips;
//   * its holder is stripped (RemoveAllItems: round start, player_weaponstrip, plugins): back to its spot;
//   * its holder leaves, becomes a spectator or a Game Master: back to its spot at once;
//   * a round restart: every personal item back home before the players spawn; a shared one only with round_reset 1;
//   * player_inventory takes it (Remove / Set): it is used up (hidden) until it goes back home at a round restart
//     (personal) or with round_reset 1 (shared).
// Counts player_inventory gives have no entity: they are gone at the holder's death, strip, leave and at a round
// restart (personal), or stay in the shared pool (round_reset 1 on the player_inventory takes them back).
//
// No entity is created at run time; the model and sound are precached in Precache whatever the keys say.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"items.h"
#include	"gamerules.h"
#include	"saverestore.h"
#include	"round_hooks.h"
#include	"client_sendqueue.h"
#include	"inventory.h"

extern int gmsgInventory;
extern bool g_worldExists;	// cbase.cpp: a map is running

#define SF_INVENTORY_SHARED			4	// item_pickup, player_inventory, player_hasinventory: the team's shared pool

#define INV_DEFAULT_MODEL			"models/w_security.mdl"	// Half-Life's security card (valve/)
#define INV_DEFAULT_NOISE			"items/gunpickup2.wav"
#define INV_DEFAULT_RETURN_TIME		45.0f
#define INV_DEFAULT_ICON			"inv_key"	// item_icon left out (the FGD default)
#define INV_RETURN_TIME_MAX			3600.0f

#define INV_QUEUE_DELAY				0.5
#define INV_QUEUE_INTERVAL			0.1
#define INV_QUEUE_BYTES				400
#define INV_MSG_HEADER				3		// message id + size byte, as client_sendqueue users count it

enum
{
	INVENTORY_ITEM_NONE_GIVEN_MAXITEMS = -2,	// no free slot
	INVENTORY_ITEM_NONE_GIVEN_MAXCOUNT = -1,	// at its maximum already
	INVENTORY_ITEM_NO_CHANGE = 0,
	INVENTORY_ITEM_COUNT_CHANGED = 1,
	INVENTORY_ITEM_GIVEN = 2,
	INVENTORY_ITEM_GIVEN_OVERFLOW = 3,			// given, and it went over its maximum
};

typedef struct
{
	char	name[ INVENTORY_NAME_LEN ];		// "" = a free slot
	char	icon[ INVENTORY_NAME_LEN ];
	int		count;							// 1 .. INVENTORY_COUNT_MAX while used
} inv_slot_t;

typedef struct
{
	inv_slot_t	slots[ INVENTORY_SLOTS ];
} inv_list_t;

static inv_list_t s_PlayerLists[ 33 ];	// by client index 1 .. 32
static inv_list_t s_Pool;				// the shared pool

//=========================================================
// The lists
//=========================================================

// bounded copy (names and icons: at most INVENTORY_NAME_LEN - 1 characters)
static void Inv_Copy( char *out, const char *in )
{
	if ( !in )
		in = "";
	strncpy( out, in, INVENTORY_NAME_LEN - 1 );
	out[ INVENTORY_NAME_LEN - 1 ] = '\0';
}

static inv_list_t *Inv_List( CBasePlayer *pOwner )
{
	if ( !pOwner )
		return &s_Pool;
	const int i = pOwner->entindex();
	if ( i < 1 || i > 32 )
		return NULL;
	return &s_PlayerLists[i];
}

static int Inv_Find( const inv_list_t *list, const char *pszItem )
{
	if ( !list || !pszItem || !pszItem[0] )
		return -1;
	for ( int i = 0; i < INVENTORY_SLOTS; i++ )
		if ( list->slots[i].name[0] && !strncmp( list->slots[i].name, pszItem, INVENTORY_NAME_LEN - 1 ) )
			return i;
	return -1;
}

static int Inv_MsgBytes( const inv_slot_t &slot )
{
	return INV_MSG_HEADER + 2 + (int)strlen( slot.name ) + 1 + (int)strlen( slot.icon ) + 1 + 1;
}

static void Inv_Write( const inv_slot_t &slot, BOOL bShared )
{
	WRITE_SHORT( slot.count );
	WRITE_STRING( slot.name );
	WRITE_STRING( slot.icon );
	WRITE_BYTE( bShared ? INVENTORY_FLAG_SHARED : 0 );
}

// the new count of one item to its owner (the shared pool: to everybody)
static void Inv_Send( CBasePlayer *pOwner, const inv_slot_t &slot )
{
	if ( !gmsgInventory || !g_worldExists )
		return;
	if ( pOwner )
	{
		if ( pOwner->has_disconnected || !pOwner->pev || FNullEnt( pOwner->edict() ) )
			return;
		MESSAGE_BEGIN( MSG_ONE, gmsgInventory, NULL, pOwner->edict() );
			Inv_Write( slot, FALSE );
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgInventory );
			Inv_Write( slot, TRUE );
		MESSAGE_END();
	}
}

// sets a slot's count (0 frees it) and tells the owner
static void Inv_SetSlot( CBasePlayer *pOwner, inv_list_t *list, int i, int count )
{
	inv_slot_t &slot = list->slots[i];
	count = max( 0, min( count, INVENTORY_COUNT_MAX ) );
	if ( slot.count == count )
		return;
	slot.count = count;
	Inv_Send( pOwner, slot );
	if ( !count )
		memset( &slot, 0, sizeof( slot ) );
}

static void Inv_ClearList( CBasePlayer *pOwner, inv_list_t *list )
{
	for ( int i = 0; i < INVENTORY_SLOTS; i++ )
		if ( list->slots[i].name[0] )
			Inv_SetSlot( pOwner, list, i, 0 );
}

// Featureful CBasePlayer::GiveInventoryItem: iMax > 0 is the item's maximum (collect_limit); bOverflow lets the
// count go over it
static int Inv_Give( CBasePlayer *pOwner, const char *pszItem, const char *pszIcon, int iCount, int iMax,
	BOOL bOverflow )
{
	inv_list_t *list = Inv_List( pOwner );
	if ( !list || !pszItem || !pszItem[0] )
		return INVENTORY_ITEM_NO_CHANGE;
	if ( iCount <= 0 )
		iCount = 1;

	int i = Inv_Find( list, pszItem );
	if ( i < 0 )
	{
		for ( int j = 0; j < INVENTORY_SLOTS && i < 0; j++ )
			if ( !list->slots[j].name[0] )
				i = j;
		if ( i < 0 )
			return INVENTORY_ITEM_NONE_GIVEN_MAXITEMS;
	}

	inv_slot_t &slot = list->slots[i];
	const int iOld = slot.name[0] ? slot.count : 0;
	if ( ( iMax > 0 && !bOverflow && iOld >= iMax ) || iOld >= INVENTORY_COUNT_MAX )
		return INVENTORY_ITEM_NONE_GIVEN_MAXCOUNT;

	int iNew = min( iOld + iCount, INVENTORY_COUNT_MAX );
	int result = INVENTORY_ITEM_GIVEN;
	if ( iMax > 0 && iNew > iMax )
	{
		if ( !bOverflow )
			iNew = iMax;
		result = INVENTORY_ITEM_GIVEN_OVERFLOW;
	}

	if ( !slot.name[0] )
	{
		Inv_Copy( slot.name, pszItem );
		Inv_Copy( slot.icon, pszIcon );
	}
	else if ( !slot.icon[0] && pszIcon && pszIcon[0] )
		Inv_Copy( slot.icon, pszIcon );
	Inv_SetSlot( pOwner, list, i, iNew );
	return result;
}

static void Inv_SpendTokens( CBasePlayer *pOwner, const char *pszItem );

// Featureful CBasePlayer::RemoveInventoryItem
static BOOL Inv_Remove( CBasePlayer *pOwner, const char *pszItem, int iCount )
{
	inv_list_t *list = Inv_List( pOwner );
	const int i = Inv_Find( list, pszItem );
	if ( i < 0 )
		return FALSE;
	char szItem[ INVENTORY_NAME_LEN ];
	Inv_Copy( szItem, pszItem );
	if ( iCount <= 0 )
		iCount = 1;
	Inv_SetSlot( pOwner, list, i, list->slots[i].count - iCount );
	Inv_SpendTokens( pOwner, szItem );
	return TRUE;
}

// Featureful CBasePlayer::SetInventoryItem
static int Inv_SetCount( CBasePlayer *pOwner, const char *pszItem, const char *pszIcon, int iCount, int iMax,
	BOOL bOverflow )
{
	inv_list_t *list = Inv_List( pOwner );
	const int i = Inv_Find( list, pszItem );
	if ( i < 0 )
		return iCount > 0 ? Inv_Give( pOwner, pszItem, pszIcon, iCount, iMax, bOverflow ) : INVENTORY_ITEM_NO_CHANGE;

	const int iOld = list->slots[i].count;
	int iNew = max( 0, min( iCount, INVENTORY_COUNT_MAX ) );
	if ( iNew == iOld )
		return INVENTORY_ITEM_NO_CHANGE;
	int result = INVENTORY_ITEM_COUNT_CHANGED;
	if ( iMax > 0 && iOld <= iMax && iNew > iMax )
	{
		if ( !bOverflow )
			iNew = iMax;
		result = INVENTORY_ITEM_GIVEN_OVERFLOW;
	}
	char szItem[ INVENTORY_NAME_LEN ];
	Inv_Copy( szItem, pszItem );
	Inv_SetSlot( pOwner, list, i, iNew );
	if ( iNew < iOld )
		Inv_SpendTokens( pOwner, szItem );
	return result;
}

int Inventory_Count( CBasePlayer *pPlayer, const char *pszItem )
{
	const inv_list_t *list = Inv_List( pPlayer );
	const int i = Inv_Find( list, pszItem );
	return i < 0 ? 0 : list->slots[i].count;
}

//=========================================================
// item_pickup: the item itself (Featureful CItemPickup keys, SevenKewp CItemInventory's carry model)
//=========================================================
enum
{
	INV_STATE_HOME = 0,		// at its placed spot, can be taken
	INV_STATE_HELD,			// a player holds it (hidden)
	INV_STATE_POOL,			// in the shared pool (hidden)
	INV_STATE_DROPPED,		// lying where its holder died, can be taken, goes home after return_timelimit
	INV_STATE_SPENT,		// player_inventory took it (hidden) until it goes home
};

class CItemPickup : public CItem
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	EXPORT PickupTouch( CBaseEntity *pOther );
	void	EXPORT PickupThink( void );
	void	RoundReset( void );
	CKeyValue	GetKeyValue( const char *keyName );
	BOOL	CanShowObjectHint( void ) { return ( m_iState == INV_STATE_HOME || m_iState == INV_STATE_DROPPED ) && CItem::CanShowObjectHint(); }

	BOOL	Shared( void ) { return FBitSet( pev->spawnflags, SF_INVENTORY_SHARED ) != 0; }
	BOOL	TryPickup( CBasePlayer *pPlayer );
	void	Hide( void );
	void	ReturnHome( void );
	void	DropAt( const Vector &vecOrigin, int iIndex );
	void	Spend( void );
	BOOL	HeldBy( CBasePlayer *pOwner );	// pOwner NULL: in the shared pool

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int		m_iState;
	EHANDLE	m_hHolder;
	Vector	m_vecHome;
	Vector	m_vecHomeAngles;
	float	m_flReturnTime;
	float	m_flReturnLimit;	// return_timelimit
	int		m_iCollectLimit;	// collect_limit (0: no limit)
	int		m_iCount;			// count (1 .. INVENTORY_COUNT_MAX)
	BOOL	m_bKeepOnDeath;		// holder_keep_on_death
	BOOL	m_bRoundReset;		// round_reset
	BOOL	m_bSpawned;			// keys that need a precache are ignored after this
	BOOL	m_bDefaultsSet;
	char	m_szName[ INVENTORY_NAME_LEN ];	// item_name
	char	m_szIcon[ INVENTORY_NAME_LEN ];	// item_icon
};

LINK_ENTITY_TO_CLASS( item_pickup, CItemPickup );

TYPEDESCRIPTION CItemPickup::m_SaveData[] =
{
	DEFINE_FIELD( CItemPickup, m_iState, FIELD_INTEGER ),
	DEFINE_FIELD( CItemPickup, m_hHolder, FIELD_EHANDLE ),
	DEFINE_FIELD( CItemPickup, m_vecHome, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CItemPickup, m_vecHomeAngles, FIELD_VECTOR ),
	DEFINE_FIELD( CItemPickup, m_flReturnTime, FIELD_TIME ),
	DEFINE_FIELD( CItemPickup, m_flReturnLimit, FIELD_FLOAT ),
	DEFINE_FIELD( CItemPickup, m_iCollectLimit, FIELD_INTEGER ),
	DEFINE_FIELD( CItemPickup, m_iCount, FIELD_INTEGER ),
	DEFINE_FIELD( CItemPickup, m_bKeepOnDeath, FIELD_BOOLEAN ),
	DEFINE_FIELD( CItemPickup, m_bRoundReset, FIELD_BOOLEAN ),
	DEFINE_FIELD( CItemPickup, m_bSpawned, FIELD_BOOLEAN ),
	DEFINE_FIELD( CItemPickup, m_bDefaultsSet, FIELD_BOOLEAN ),
	DEFINE_ARRAY( CItemPickup, m_szName, FIELD_CHARACTER, INVENTORY_NAME_LEN ),
	DEFINE_ARRAY( CItemPickup, m_szIcon, FIELD_CHARACTER, INVENTORY_NAME_LEN ),
};

IMPLEMENT_SAVERESTORE( CItemPickup, CItem );

static void Inv_Defaults( CItemPickup *p )
{
	if ( p->m_bDefaultsSet )
		return;
	p->m_bDefaultsSet = TRUE;
	p->m_flReturnLimit = INV_DEFAULT_RETURN_TIME;
	p->m_iCount = 1;
	Inv_Copy( p->m_szIcon, INV_DEFAULT_ICON );	// item_icon left out: the FGD default
}

void CItemPickup::KeyValue( KeyValueData *pkvd )
{
	Inv_Defaults( this );
	if ( m_bSpawned && EntvarsKeyNeedsPrecache( pkvd->szKeyName ) )
	{
		pkvd->fHandled = TRUE;	// the model and the sound are precached at map load only: a change later is ignored
		return;
	}
	if ( FStrEq( pkvd->szKeyName, "item_name" ) )
	{
		if ( !m_bSpawned )
		{
			if ( strlen( pkvd->szValue ) >= INVENTORY_NAME_LEN )
				ALERT( at_console, "item_pickup: item_name \"%s\" cut to %d characters\n", pkvd->szValue, INVENTORY_NAME_LEN - 1 );
			Inv_Copy( m_szName, pkvd->szValue );
		}
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "item_icon" ) )
	{
		Inv_Copy( m_szIcon, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "count" ) )
	{
		m_iCount = max( 1, min( atoi( pkvd->szValue ), INVENTORY_COUNT_MAX ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "collect_limit" ) )
	{
		m_iCollectLimit = max( 0, min( atoi( pkvd->szValue ), INVENTORY_COUNT_MAX ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "return_timelimit" ) )
	{
		float f = atof( pkvd->szValue );
		if ( !( f >= 0.0f ) )	// negative or not a number
			f = 0.0f;
		m_flReturnLimit = min( f, INV_RETURN_TIME_MAX );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "holder_keep_on_death" ) )
	{
		m_bKeepOnDeath = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "round_reset" ) )
	{
		m_bRoundReset = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CItem::KeyValue( pkvd );
}

void CItemPickup::Precache( void )
{
	if ( FStringNull( pev->model ) || !STRING( pev->model )[0] )
		pev->model = MAKE_STRING( INV_DEFAULT_MODEL );
	if ( FStringNull( pev->noise ) || !STRING( pev->noise )[0] )
		pev->noise = MAKE_STRING( INV_DEFAULT_NOISE );
	PRECACHE_MODEL( (char *)STRING( pev->model ) );
	PRECACHE_SOUND( (char *)STRING( pev->noise ) );
}

void CItemPickup::Spawn( void )
{
	Inv_Defaults( this );
	if ( !m_szName[0] )
		ALERT( at_console, "item_pickup without an item_name at %.0f %.0f %.0f: nobody can take it\n",
			pev->origin.x, pev->origin.y, pev->origin.z );

	Precache();
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	CItem::Spawn();
	m_bSpawned = TRUE;
	if ( FBitSet( pev->flags, FL_KILLME ) )
		return;	// fell out of the level (CItem::Spawn removed it)

	SetTouch( &CItemPickup::PickupTouch );
	m_vecHome = pev->origin;
	m_vecHomeAngles = pev->angles;
	m_iState = INV_STATE_HOME;
	m_hHolder = NULL;
}

void CItemPickup::PickupTouch( CBaseEntity *pOther )
{
	// as CItem::ItemTouch: a "USE only" item ignores touch
	if ( FBitSet( pev->spawnflags, SF_ITEM_USE_ONLY ) && !FBitSet( pev->spawnflags, SF_ITEM_TOUCH_ONLY ) )
		return;
	if ( pOther && pOther->IsPlayer() )
		TryPickup( (CBasePlayer *)pOther );
}

void CItemPickup::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// as CItem::Use: only a "USE only" item, by the player who pressed +use
	if ( !FBitSet( pev->spawnflags, SF_ITEM_USE_ONLY ) || FBitSet( pev->spawnflags, SF_ITEM_TOUCH_ONLY ) )
		return;
	if ( pCaller && pCaller->IsPlayer() )
		TryPickup( (CBasePlayer *)pCaller );
}

BOOL CItemPickup::HeldBy( CBasePlayer *pOwner )
{
	if ( !pOwner )
		return m_iState == INV_STATE_POOL;
	return m_iState == INV_STATE_HELD && (CBaseEntity *)m_hHolder == pOwner;
}

BOOL CItemPickup::TryPickup( CBasePlayer *pPlayer )
{
	// taken already (also by another player in this frame), used up, removed, or no name
	if ( m_iState != INV_STATE_HOME && m_iState != INV_STATE_DROPPED )
		return FALSE;
	if ( FBitSet( pev->effects, EF_NODRAW ) || FBitSet( pev->flags, FL_KILLME ) || !m_szName[0] )
		return FALSE;
	// spectators, Game Masters, dead and waiting players never take items
	if ( !Logic_IsPlayerInPlay( pPlayer ) || !g_pGameRules->CanHaveItem( pPlayer, this ) )
		return FALSE;

	CBasePlayer *pOwner = Shared() ? NULL : pPlayer;
	const int iHave = Inventory_Count( pOwner, m_szName );
	if ( m_iCollectLimit > 0 && iHave + m_iCount > m_iCollectLimit )
		return FALSE;	// collect_limit: the player (the pool) can't carry more of it
	if ( iHave + m_iCount > INVENTORY_COUNT_MAX )
		return FALSE;
	if ( Inv_Give( pOwner, m_szName, m_szIcon, m_iCount, 0, FALSE ) < INVENTORY_ITEM_GIVEN )
		return FALSE;	// no free slot

	m_iState = Shared() ? INV_STATE_POOL : INV_STATE_HELD;
	m_hHolder = Shared() ? NULL : (CBaseEntity *)pPlayer;
	Hide();
	if ( !Shared() )
	{
		// a held item checks now and then that its holder is still there (a slot freed without ClientDisconnect)
		SetThink( &CItemPickup::PickupThink );
		pev->nextthink = gpGlobals->time + 1.0f;
	}

	EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, STRING( pev->noise ), 1, ATTN_NORM );
	if ( !FStringNull( pev->message ) && STRING( pev->message )[0] )
		UTIL_ShowMessage( STRING( pev->message ), pPlayer );
	SUB_UseTargets( pPlayer, USE_TOGGLE, 0 );
	return TRUE;
}

void CItemPickup::Hide( void )
{
	SetTouch( NULL );
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	UTIL_SetOrigin( pev, m_vecHome );
	SetThink( NULL );
	pev->nextthink = 0;
}

void CItemPickup::ReturnHome( void )
{
	m_iState = INV_STATE_HOME;
	m_hHolder = NULL;
	m_flReturnTime = 0;
	pev->effects &= ~EF_NODRAW;
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	pev->angles = m_vecHomeAngles;
	ClearBits( pev->flags, FL_ONGROUND );
	UTIL_SetOrigin( pev, m_vecHome );
	SetTouch( &CItemPickup::PickupTouch );
	SetThink( NULL );
	pev->nextthink = 0;
}

void CItemPickup::DropAt( const Vector &vecOrigin, int iIndex )
{
	m_hHolder = NULL;
	if ( m_flReturnLimit <= 0 )
	{
		ReturnHome();	// return_timelimit 0: straight back
		return;
	}
	m_iState = INV_STATE_DROPPED;
	m_flReturnTime = gpGlobals->time + m_flReturnLimit;
	pev->effects &= ~EF_NODRAW;
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	ClearBits( pev->flags, FL_ONGROUND );
	UTIL_SetOrigin( pev, vecOrigin );
	// several items fan out a little (no randomness: the same every time)
	const float flYaw = iIndex * 72.0f * ( 3.14159265f / 180.0f );
	pev->velocity = Vector( cos( flYaw ) * 60.0f * ( iIndex ? 1 : 0 ), sin( flYaw ) * 60.0f * ( iIndex ? 1 : 0 ), 150.0f );
	pev->avelocity = g_vecZero;
	SetTouch( &CItemPickup::PickupTouch );
	SetThink( &CItemPickup::PickupThink );
	pev->nextthink = gpGlobals->time + 0.5f;
}

void CItemPickup::Spend( void )
{
	m_iState = INV_STATE_SPENT;
	m_hHolder = NULL;
	Hide();
}

void CItemPickup::PickupThink( void )
{
	if ( m_iState == INV_STATE_HELD )
	{
		CBaseEntity *pHolder = m_hHolder;
		if ( !pHolder || !pHolder->IsPlayer() || ( (CBasePlayer *)pHolder )->has_disconnected )
		{
			ReturnHome();
			return;
		}
		pev->nextthink = gpGlobals->time + 1.0f;
		return;
	}
	if ( m_iState != INV_STATE_DROPPED )
		return;

	// out of reach: in a wall, the sky, lava or slime; or untouched for return_timelimit seconds
	const int iContents = UTIL_PointContents( pev->origin );
	if ( iContents == CONTENTS_SOLID || iContents == CONTENTS_SKY || iContents == CONTENTS_LAVA
		|| iContents == CONTENTS_SLIME || gpGlobals->time >= m_flReturnTime )
	{
		ReturnHome();
		return;
	}
	pev->nextthink = gpGlobals->time + 0.5f;
}

void CItemPickup::RoundReset( void )
{
	if ( !Shared() )
	{
		if ( m_iState != INV_STATE_HOME )
			ReturnHome();	// Inventory_RoundReset did it already; an item that was lying somewhere too
		return;
	}
	if ( !m_bRoundReset || m_iState == INV_STATE_HOME )
		return;
	const BOOL bInPool = ( m_iState == INV_STATE_POOL );
	ReturnHome();
	if ( bInPool )
		Inv_Remove( NULL, m_szName, m_iCount );	// the pool gives back what this item added
}

CKeyValue CItemPickup::GetKeyValue( const char *keyName )
{
	CKeyValue keyvalue = g_emptyKeyValue;
	if ( FStrEq( keyName, "inv_state" ) )
	{
		keyvalue.keyType = KEY_TYPE_INT;
		keyvalue.keyName = "inv_state";
		keyvalue.iVal = m_iState;
		return keyvalue;
	}
	if ( FStrEq( keyName, "inv_holder" ) )
	{
		CBaseEntity *pHolder = m_hHolder;
		keyvalue.keyType = KEY_TYPE_INT;
		keyvalue.keyName = "inv_holder";
		keyvalue.iVal = ( m_iState == INV_STATE_HELD && pHolder ) ? pHolder->entindex() : 0;
		return keyvalue;
	}
	return CItem::GetKeyValue( keyName );
}

// every item_pickup on the map, in entity order
static CItemPickup *Inv_NextToken( CItemPickup *pAfter )
{
	CBaseEntity *p = pAfter;
	while ( ( p = UTIL_FindEntityByClassname( p, "item_pickup" ) ) != NULL )
	{
		if ( !FBitSet( p->pev->flags, FL_KILLME ) )
			return (CItemPickup *)p;
	}
	return NULL;
}

// a list's count went down: items of that name the owner holds beyond the count are used up (last ones first)
static void Inv_SpendTokens( CBasePlayer *pOwner, const char *pszItem )
{
	int iHeld = 0;
	for ( CItemPickup *p = Inv_NextToken( NULL ); p; p = Inv_NextToken( p ) )
		if ( p->HeldBy( pOwner ) && !strcmp( p->m_szName, pszItem ) )
			iHeld += p->m_iCount;

	const int iCount = Inventory_Count( pOwner, pszItem );
	while ( iHeld > iCount )
	{
		CItemPickup *pLast = NULL;
		for ( CItemPickup *p = Inv_NextToken( NULL ); p; p = Inv_NextToken( p ) )
			if ( p->HeldBy( pOwner ) && !strcmp( p->m_szName, pszItem ) )
				pLast = p;
		if ( !pLast )
			break;
		iHeld -= pLast->m_iCount;
		pLast->Spend();
	}
}

// a player's list again from the items he still holds (what player_inventory gave him is gone)
static void Inv_Rebuild( CBasePlayer *pPlayer )
{
	inv_list_t *list = Inv_List( pPlayer );
	if ( !list )
		return;
	for ( int i = 0; i < INVENTORY_SLOTS; i++ )
	{
		if ( !list->slots[i].name[0] )
			continue;
		int iHeld = 0;
		for ( CItemPickup *p = Inv_NextToken( NULL ); p; p = Inv_NextToken( p ) )
			if ( p->HeldBy( pPlayer ) && !strcmp( p->m_szName, list->slots[i].name ) )
				iHeld += p->m_iCount;
		Inv_SetSlot( pPlayer, list, i, iHeld );
	}
}

//=========================================================
// Players
//=========================================================
void Inventory_MapStart( void )
{
	memset( s_PlayerLists, 0, sizeof( s_PlayerLists ) );
	memset( &s_Pool, 0, sizeof( s_Pool ) );
}

void Inventory_ClientPutInServer( CBasePlayer *pPlayer )
{
	inv_list_t *list = Inv_List( pPlayer );
	if ( list )
		memset( list, 0, sizeof( *list ) );
}

void Inventory_PlayerKilled( CBasePlayer *pPlayer )
{
	if ( !Inv_List( pPlayer ) )
		return;
	int iDropped = 0;
	for ( CItemPickup *p = Inv_NextToken( NULL ); p; p = Inv_NextToken( p ) )
	{
		if ( !p->HeldBy( pPlayer ) || p->m_bKeepOnDeath )
			continue;
		p->DropAt( pPlayer->pev->origin, iDropped++ );
	}
	Inv_Rebuild( pPlayer );
}

void Inventory_Strip( CBasePlayer *pPlayer )
{
	if ( !Inv_List( pPlayer ) )
		return;
	for ( CItemPickup *p = Inv_NextToken( NULL ); p; p = Inv_NextToken( p ) )
		if ( p->HeldBy( pPlayer ) && !p->m_bKeepOnDeath )
			p->ReturnHome();
	Inv_Rebuild( pPlayer );
}

void Inventory_ReturnAll( CBasePlayer *pPlayer )
{
	inv_list_t *list = Inv_List( pPlayer );
	if ( !list )
		return;
	for ( CItemPickup *p = Inv_NextToken( NULL ); p; p = Inv_NextToken( p ) )
		if ( p->HeldBy( pPlayer ) )
			p->ReturnHome();
	Inv_ClearList( pPlayer, list );
}

void Inventory_ClientDisconnected( CBasePlayer *pPlayer )
{
	inv_list_t *list = Inv_List( pPlayer );
	if ( !list )
		return;
	for ( CItemPickup *p = Inv_NextToken( NULL ); p; p = Inv_NextToken( p ) )
		if ( p->HeldBy( pPlayer ) )
			p->ReturnHome();
	memset( list, 0, sizeof( *list ) );	// nothing to tell a client that is leaving
}

void Inventory_RoundReset( void )
{
	for ( CItemPickup *p = Inv_NextToken( NULL ); p; p = Inv_NextToken( p ) )
		if ( !p->Shared() && p->m_iState != INV_STATE_HOME )
			p->ReturnHome();
	for ( int i = 1; i <= gpGlobals->maxClients && i <= 32; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( pPlayer )
			Inv_ClearList( pPlayer, &s_PlayerLists[i] );
		else
			memset( &s_PlayerLists[i], 0, sizeof( s_PlayerLists[i] ) );
	}
}

BOOL Inventory_TestChange( CBasePlayer *pPlayer, const char *pszItem, int iCount )
{
	if ( !pszItem || !pszItem[0] || strlen( pszItem ) >= INVENTORY_NAME_LEN || !iCount )
		return FALSE;
	if ( pPlayer && !Logic_IsPlayerInPlay( pPlayer ) )
		return FALSE;
	if ( iCount > 0 )
		return Inv_Give( pPlayer, pszItem, "", min( iCount, INVENTORY_COUNT_MAX ), 0, FALSE ) >= INVENTORY_ITEM_GIVEN;
	return Inv_Remove( pPlayer, pszItem, min( -iCount, INVENTORY_COUNT_MAX ) );
}

// the send queue (client_sendqueue.h, SENDQUEUE_INVENTORY): the player's own items (cursor 0 .. 15), then the
// shared pool (16 .. 31)
static int Inv_SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget )
{
	inv_list_t *own = Inv_List( pPlayer );
	for ( int i = max( iCursor, 0 ); i < INVENTORY_SLOTS * 2; i++ )
	{
		const BOOL bShared = i >= INVENTORY_SLOTS;
		const inv_list_t *list = bShared ? &s_Pool : own;
		if ( !list )
			continue;
		const inv_slot_t &slot = list->slots[ i % INVENTORY_SLOTS ];
		if ( !slot.name[0] )
			continue;
		const int bytes = Inv_MsgBytes( slot );
		if ( bytes > iBudget )
		{
			iCursor = i;
			return FALSE;
		}
		MESSAGE_BEGIN( MSG_ONE, gmsgInventory, NULL, pPlayer->edict() );
			Inv_Write( slot, bShared );
		MESSAGE_END();
		iBudget -= bytes;
	}
	return TRUE;
}

static SendQueue_Sender s_InventorySender( SENDQUEUE_INVENTORY, Inv_SendQueued, 0, INV_QUEUE_DELAY,
	INV_QUEUE_INTERVAL, INV_QUEUE_BYTES );

//=========================================================
// player_inventory (Featureful): gives, takes or sets an item of the activator (Shared: of the shared pool)
//=========================================================
#define SF_PLAYER_INVENTORY_REMOVE_ON_FIRE		1
#define SF_PLAYER_INVENTORY_ALLOW_OVERFLOW		2

#define PLAYER_INVENTORY_OP_REMOVE	-1
#define PLAYER_INVENTORY_OP_INPUT	0
#define PLAYER_INVENTORY_OP_GIVE	1
#define PLAYER_INVENTORY_OP_SET		2

class CPlayerInventory : public CPointEntity
{
public:
	void	Spawn( void ) { Defaults(); CPointEntity::Spawn(); }
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	RoundReset( void );

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t	m_iszOnCountChange;
	string_t	m_iszOnItemLimit;
	string_t	m_iszOnMaxCountLimit;
	string_t	m_iszOnCountOverflow;
	int			m_iCollectLimit;
	BOOL		m_bRoundReset;
	BOOL		m_bUsedUp;		// Remove On Fire with round_reset: off until the next round
	char		m_szName[ INVENTORY_NAME_LEN ];
	char		m_szIcon[ INVENTORY_NAME_LEN ];
	BOOL		m_bDefaultsSet;	// the FGD defaults given before the first key (count 1, item_icon inv_key)

	// keys left out are the FGD defaults: Count 1, HUD icon inv_key (before the first key, or at Spawn)
	void	Defaults( void )
	{
		if ( m_bDefaultsSet )
			return;
		m_bDefaultsSet = TRUE;
		pev->impulse = 1;
		Inv_Copy( m_szIcon, INV_DEFAULT_ICON );
	}
};

LINK_ENTITY_TO_CLASS( player_inventory, CPlayerInventory );

TYPEDESCRIPTION CPlayerInventory::m_SaveData[] =
{
	DEFINE_FIELD( CPlayerInventory, m_iszOnCountChange, FIELD_STRING ),
	DEFINE_FIELD( CPlayerInventory, m_iszOnItemLimit, FIELD_STRING ),
	DEFINE_FIELD( CPlayerInventory, m_iszOnMaxCountLimit, FIELD_STRING ),
	DEFINE_FIELD( CPlayerInventory, m_iszOnCountOverflow, FIELD_STRING ),
	DEFINE_FIELD( CPlayerInventory, m_iCollectLimit, FIELD_INTEGER ),
	DEFINE_FIELD( CPlayerInventory, m_bRoundReset, FIELD_BOOLEAN ),
	DEFINE_FIELD( CPlayerInventory, m_bUsedUp, FIELD_BOOLEAN ),
	DEFINE_ARRAY( CPlayerInventory, m_szName, FIELD_CHARACTER, INVENTORY_NAME_LEN ),
	DEFINE_ARRAY( CPlayerInventory, m_szIcon, FIELD_CHARACTER, INVENTORY_NAME_LEN ),
	DEFINE_FIELD( CPlayerInventory, m_bDefaultsSet, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CPlayerInventory, CPointEntity );

void CPlayerInventory::KeyValue( KeyValueData *pkvd )
{
	Defaults();
	if ( FStrEq( pkvd->szKeyName, "item_name" ) )
	{
		if ( strlen( pkvd->szValue ) >= INVENTORY_NAME_LEN )
			ALERT( at_console, "player_inventory: item_name \"%s\" cut to %d characters\n", pkvd->szValue, INVENTORY_NAME_LEN - 1 );
		Inv_Copy( m_szName, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "item_icon" ) )
	{
		Inv_Copy( m_szIcon, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "operation" ) )
	{
		pev->weapons = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "count" ) )
	{
		pev->impulse = max( -INVENTORY_COUNT_MAX, min( atoi( pkvd->szValue ), INVENTORY_COUNT_MAX ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "collect_limit" ) )
	{
		m_iCollectLimit = max( 0, min( atoi( pkvd->szValue ), INVENTORY_COUNT_MAX ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "on_count_change" ) )
	{
		m_iszOnCountChange = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "on_item_limit" ) )
	{
		m_iszOnItemLimit = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "on_max_count_limit" ) )
	{
		m_iszOnMaxCountLimit = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "on_count_overflow" ) )
	{
		m_iszOnCountOverflow = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "round_reset" ) )
	{
		m_bRoundReset = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CPlayerInventory::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_bUsedUp )
		return;

	// Featureful: EffectiveAlivePlayer( activator ). SBO: a player in play; Shared: the pool, any activator
	CBasePlayer *pOwner = NULL;
	if ( !FBitSet( pev->spawnflags, SF_INVENTORY_SHARED ) )
	{
		if ( !Logic_IsPlayerInPlay( pActivator ) )
			return;
		pOwner = (CBasePlayer *)pActivator;
	}

	int operation = pev->weapons;
	int count = pev->impulse;
	if ( operation == PLAYER_INVENTORY_OP_INPUT )
	{
		if ( useType == USE_OFF )
			operation = PLAYER_INVENTORY_OP_REMOVE;
		else if ( useType == USE_SET )
		{
			operation = PLAYER_INVENTORY_OP_SET;
			count = ( value >= INVENTORY_COUNT_MAX ) ? INVENTORY_COUNT_MAX : ( value <= -INVENTORY_COUNT_MAX ) ? -INVENTORY_COUNT_MAX : (int)value;
		}
		else
			operation = PLAYER_INVENTORY_OP_GIVE;
	}

	if ( !m_szName[0] )
	{
		// no item name: Set or Remove with a count of 0 or less takes every item
		if ( count <= 0 && ( operation == PLAYER_INVENTORY_OP_SET || operation == PLAYER_INVENTORY_OP_REMOVE ) )
		{
			inv_list_t *list = Inv_List( pOwner );
			for ( int i = 0; list && i < INVENTORY_SLOTS; i++ )
			{
				if ( !list->slots[i].name[0] )
					continue;
				char szItem[ INVENTORY_NAME_LEN ];
				Inv_Copy( szItem, list->slots[i].name );
				Inv_SetSlot( pOwner, list, i, 0 );
				Inv_SpendTokens( pOwner, szItem );
			}
		}
		return;
	}

	const int oldCount = Inventory_Count( pOwner, m_szName );
	const BOOL bOverflow = FBitSet( pev->spawnflags, SF_PLAYER_INVENTORY_ALLOW_OVERFLOW ) != 0;
	int result = INVENTORY_ITEM_NO_CHANGE;
	switch ( operation )
	{
	case PLAYER_INVENTORY_OP_REMOVE:
		result = Inv_Remove( pOwner, m_szName, count ) ? INVENTORY_ITEM_COUNT_CHANGED : INVENTORY_ITEM_NO_CHANGE;
		break;
	case PLAYER_INVENTORY_OP_SET:
		result = Inv_SetCount( pOwner, m_szName, m_szIcon, count, m_iCollectLimit, bOverflow );
		break;
	case PLAYER_INVENTORY_OP_GIVE:
		result = Inv_Give( pOwner, m_szName, m_szIcon, count, m_iCollectLimit, bOverflow );
		break;
	default:
		break;
	}

	if ( !FStringNull( m_iszOnCountChange ) && Inventory_Count( pOwner, m_szName ) != oldCount )
		FireTargets( STRING( m_iszOnCountChange ), pActivator, this, USE_TOGGLE, 0 );

	switch ( result )
	{
	case INVENTORY_ITEM_NONE_GIVEN_MAXITEMS:
		if ( !FStringNull( m_iszOnItemLimit ) )
			FireTargets( STRING( m_iszOnItemLimit ), pActivator, this, USE_TOGGLE, 0 );
		break;
	case INVENTORY_ITEM_NONE_GIVEN_MAXCOUNT:
		if ( !FStringNull( m_iszOnMaxCountLimit ) )
			FireTargets( STRING( m_iszOnMaxCountLimit ), pActivator, this, USE_TOGGLE, 0 );
		break;
	case INVENTORY_ITEM_GIVEN_OVERFLOW:
		if ( !FStringNull( m_iszOnCountOverflow ) )
			FireTargets( STRING( m_iszOnCountOverflow ), pActivator, this, USE_TOGGLE, 0 );
		break;
	default:
		break;
	}

	if ( FBitSet( pev->spawnflags, SF_PLAYER_INVENTORY_REMOVE_ON_FIRE ) )
	{
		if ( m_bRoundReset )
			m_bUsedUp = TRUE;	// back at the next round start
		else
			UTIL_Remove( this );
	}
}

void CPlayerInventory::RoundReset( void )
{
	if ( !m_bRoundReset )
		return;
	m_bUsedUp = FALSE;
	if ( !FBitSet( pev->spawnflags, SF_INVENTORY_SHARED ) || !m_szName[0] )
		return;	// personal lists are emptied at every round start anyway

	// the shared pool keeps only what item_pickup entities in it add (what player_inventory gave is taken back)
	inv_list_t *list = &s_Pool;
	const int i = Inv_Find( list, m_szName );
	if ( i < 0 )
		return;
	int iHeld = 0;
	for ( CItemPickup *p = Inv_NextToken( NULL ); p; p = Inv_NextToken( p ) )
		if ( p->HeldBy( NULL ) && !strcmp( p->m_szName, m_szName ) )
			iHeld += p->m_iCount;
	Inv_SetSlot( NULL, list, i, iHeld );
}

//=========================================================
// player_hasinventory (Featureful): fires pass_target if the activator has the item (Shared: if the shared pool
// has it), else fail_target. As a master: on while that is so.
//=========================================================
class CPlayerHasInventory : public CPointEntity
{
public:
	void	KeyValue( KeyValueData *pkvd );
	int		ObjectCaps( void ) { return CPointEntity::ObjectCaps() | FCAP_MASTER; }
	BOOL	IsTriggered( CBaseEntity *pActivator );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t	m_iszPass;
	string_t	m_iszFail;
	char		m_szName[ INVENTORY_NAME_LEN ];
};

LINK_ENTITY_TO_CLASS( player_hasinventory, CPlayerHasInventory );

TYPEDESCRIPTION CPlayerHasInventory::m_SaveData[] =
{
	DEFINE_FIELD( CPlayerHasInventory, m_iszPass, FIELD_STRING ),
	DEFINE_FIELD( CPlayerHasInventory, m_iszFail, FIELD_STRING ),
	DEFINE_ARRAY( CPlayerHasInventory, m_szName, FIELD_CHARACTER, INVENTORY_NAME_LEN ),
};

IMPLEMENT_SAVERESTORE( CPlayerHasInventory, CPointEntity );

void CPlayerHasInventory::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "item_name" ) )
	{
		Inv_Copy( m_szName, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "pass_target" ) )
	{
		m_iszPass = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fail_target" ) )
	{
		m_iszFail = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

BOOL CPlayerHasInventory::IsTriggered( CBaseEntity *pActivator )
{
	if ( !m_szName[0] )
		return FALSE;
	if ( FBitSet( pev->spawnflags, SF_INVENTORY_SHARED ) )
		return Inventory_Count( NULL, m_szName ) > 0;
	if ( !Logic_IsPlayerInPlay( pActivator ) )
		return FALSE;
	return Inventory_Count( (CBasePlayer *)pActivator, m_szName ) > 0;
}

void CPlayerHasInventory::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// personal: only for a player in play (as player_hasweapon); Shared: any activator, the world too
	if ( !FBitSet( pev->spawnflags, SF_INVENTORY_SHARED ) && !Logic_IsPlayerInPlay( pActivator ) )
		return;
	const BOOL success = IsTriggered( pActivator );
	if ( success && !FStringNull( m_iszPass ) )
		FireTargets( STRING( m_iszPass ), pActivator, this, USE_TOGGLE, 0 );
	if ( !success && !FStringNull( m_iszFail ) )
		FireTargets( STRING( m_iszFail ), pActivator, this, USE_TOGGLE, 0 );
}
