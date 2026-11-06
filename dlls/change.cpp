// SBO: change_touch, a model brush that sets m_Changeniv when a player touches it
// (read by the coop/dm/br game rules).

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "gamerules.h"

int m_Changeniv;

class CChangeTouch : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT Touch( CBaseEntity *pOther );
};

LINK_ENTITY_TO_CLASS( change_touch, CChangeTouch );

void CChangeTouch::Spawn( void )
{
	Precache();
	SET_MODEL( ENT(pev), STRING(pev->model) );
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector(-16, -16, 0), Vector(16, 16, 72) );
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	SetTouch( Touch );
}

void CChangeTouch::Precache( void )
{
	PRECACHE_MODEL( (char *)STRING(pev->model) );
}

void CChangeTouch::Touch( CBaseEntity *pOther )
{
	if ( pOther->IsPlayer() && m_Changeniv == 0 )
		m_Changeniv = 1;
}
