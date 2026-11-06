//=========================================================
// SBO: max_round - sets the number of rounds of a map
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "gamerules.h"

int m_iMaxRound;
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
		m_iMaxRound = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}

	if ( m_iMaxRound <= 2 )
		m_iMaxRound = 5;
}
