//=========================================================
// player_calc_ratio -- a Locus number [LR] from a player: health, armor, ammo (with or without the loaded clips) or
// maximum speed, as a number or as a fraction of its maximum (docs/LOCUS.md). The player is the locus of the
// evaluation (the activator of the chain), e.g. the player who pressed the button.
//
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license) commit 30fc2f71
// (2026-10-30), dlls/player.cpp CPlayerCalcRatio. The same class name, keys and values, so Featureful entity setups
// carry over.
//
// SBO changes (marked "SBO" below):
//   * The player must be in play (round_hooks.h Logic_IsPlayerInPlay): never a spectator, a dead player or one
//     waiting for a round (Featureful: the activator when it is a player).
//   * "Inventory item count" (4) and its key item_name are not offered: SBO has no inventory items. The type fails
//     like an unknown one.
//   * Maxima from SBO: armor MAX_NORMAL_BATTERY (100, the batteries' cap); ammo the weapons' carry limit
//     (MaxAmmoCarry); maximum speed is the player's current pev->maxspeed, its fraction of the player's speed level
//     (m_iSboSpeed, 300 for a new player). Featureful: MaxArmor, GetMaxAmmo, its speed override.
//   * A fraction with a maximum of 0 fails instead of dividing by zero.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"items.h"
#include	"round_hooks.h"

enum
{
	PLAYER_CALC_PARAM_HEALTH,
	PLAYER_CALC_PARAM_ARMOR,
	PLAYER_CALC_PARAM_AMMO,
	PLAYER_CALC_PARAM_AMMO_INCLUDE_CLIP,
	PLAYER_CALC_INVENTORY_ITEM,
	PLAYER_CALC_MAXSPEED,
};

enum
{
	PLAYER_CALC_ABSOLUTE,
	PLAYER_CALC_FRACTION,
};

extern int MaxAmmoCarry( int iszName );		// weapons.cpp

class CPlayerCalcRatio : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	BOOL CalcRatio( CBaseEntity *pLocus, float *outResult );

private:
	float CalcParam( int paramType, CBasePlayer *pPlayer, BOOL isFraction, BOOL &success );
};

LINK_ENTITY_TO_CLASS( player_calc_ratio, CPlayerCalcRatio );

void CPlayerCalcRatio::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "calc_param" ) )
	{
		pev->impulse = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "value_notion" ) )
	{
		pev->button = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "ammo_name" ) )
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

BOOL CPlayerCalcRatio::CalcRatio( CBaseEntity *pLocus, float *outResult )
{
	// Featureful: g_pGameRules->EffectivePlayer( pLocus ); SBO: a player in play
	if ( !Logic_IsPlayerInPlay( pLocus ) )
		return FALSE;

	BOOL success = TRUE;
	*outResult = CalcParam( pev->impulse, (CBasePlayer *)pLocus, pev->button == PLAYER_CALC_FRACTION, success );
	return success;
}

// SBO: a fraction only of a maximum above 0
static float CalcRatio_Fraction( float value, float maximum, BOOL &success )
{
	if ( maximum <= 0 )
	{
		success = FALSE;
		return 0.0f;
	}
	return value / maximum;
}

float CPlayerCalcRatio::CalcParam( int paramType, CBasePlayer *pPlayer, BOOL isFraction, BOOL &success )
{
	success = TRUE;
	switch ( paramType )
	{
	case PLAYER_CALC_PARAM_HEALTH:
	{
		if ( pPlayer->pev->health <= 0.0f )
			return 0.0f;
		if ( isFraction )
			return CalcRatio_Fraction( pPlayer->pev->health, pPlayer->pev->max_health, success );
		return pPlayer->pev->health;
	}
	case PLAYER_CALC_PARAM_ARMOR:
	{
		if ( isFraction )
			return CalcRatio_Fraction( pPlayer->pev->armorvalue, MAX_NORMAL_BATTERY, success );
		return pPlayer->pev->armorvalue;
	}
	case PLAYER_CALC_PARAM_AMMO:
	case PLAYER_CALC_PARAM_AMMO_INCLUDE_CLIP:
	{
		if ( FStringNull( pev->message ) || !STRING( pev->message )[0] )
		{
			ALERT( at_console, "Requesting player's ammo amount via %s, but the ammo name is not specified\n", STRING( pev->classname ) );
			success = FALSE;
			return 0.0f;
		}
		const char *ammoName = STRING( pev->message );
		const int ammoIndex = CBasePlayer::GetAmmoIndex( ammoName );
		if ( ammoIndex < 0 )
		{
			success = FALSE;
			return 0.0f;
		}
		int ammoAmount = pPlayer->AmmoInventory( ammoIndex );
		int ammoMax = MaxAmmoCarry( pev->message );

		if ( paramType == PLAYER_CALC_PARAM_AMMO_INCLUDE_CLIP )
		{
			for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
			{
				for ( CBasePlayerItem *pItem = pPlayer->m_rgpPlayerItems[i]; pItem; pItem = pItem->m_pNext )
				{
					CBasePlayerWeapon *pWeapon = (CBasePlayerWeapon *)pItem->GetWeaponPtr();
					if ( !pWeapon || pWeapon->iMaxClip() == WEAPON_NOCLIP )
						continue;
					const char *ammo1 = pWeapon->pszAmmo1();
					if ( ammo1 && strcmp( ammo1, ammoName ) == 0 )
					{
						ammoAmount += pWeapon->m_iClip;
						ammoMax += pWeapon->iMaxClip();
					}
				}
			}
		}
		if ( isFraction )
			return CalcRatio_Fraction( ammoAmount, ammoMax, success );
		return ammoAmount;
	}
	case PLAYER_CALC_MAXSPEED:
	{
		const float currentMaxSpeed = pPlayer->pev->maxspeed;
		if ( isFraction )
			return CalcRatio_Fraction( currentMaxSpeed, pPlayer->m_iSboSpeed, success );
		return currentMaxSpeed;
	}
	case PLAYER_CALC_INVENTORY_ITEM:	// SBO: no inventory items
	default:
		ALERT( at_console, "Unknown player calc parameter %d in %s!\n", paramType, STRING( pev->classname ) );
		success = FALSE;
		return 0.0f;
	}
}
