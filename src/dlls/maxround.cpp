//=========================================================
// SBO: max_round - sets the number of rounds of a map
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "gamerules.h"

int g_iMapRoundLimit;
int RoundLimit;

class CMaxround : public CBaseEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
};

LINK_ENTITY_TO_CLASS( max_round, CMaxround );

void CMaxround::Spawn( void )
{
	pev->solid = SOLID_NOT;
	RoundLimit = 1;
}

void CMaxround::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "nbmaxround" ) )
	{
		g_iMapRoundLimit = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}

	// SBO 2.0: 1 and 2 are used as written (1.38: <= 2 gave 5)
	if ( g_iMapRoundLimit <= 0 )
		g_iMapRoundLimit = 5;
}
