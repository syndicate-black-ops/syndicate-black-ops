//=========================================================
// SBO: Limitator - per-map weapon limits (weap1..weap11)
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "gamerules.h"

int Limitat;
int m_Rc28;
int m_K444;
int m_Canon;
int m_Pm66;
int m_Federal;	// SBO: defined before m_Fa18 (.bss order in syndicate.so)
int m_Fa18;
int m_Spa;
int m_Ben;
int m_Sw;
int m_Nm33;
int m_Mini;

class CLimitator : public CBaseEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
};

LINK_ENTITY_TO_CLASS( Limitator, CLimitator );

// SBO 2.0: called by the world before the map's other entities spawn (CWorld::Precache). No Limitator yet, and each
// weapN starts as the reserve the loadout gives without a Limitator (coop.cpp), so a weapN a placed Limitator leaves
// out gives that reserve with the loadout and with each ammo_reserve pickup (1.38: the value of the last map that set
// it, or 1).
void Limitator_ResetForMap( void )
{
	Limitat = 0;
	m_Rc28 = 120;
	m_K444 = 140;
	m_Canon = 180;
	m_Pm66 = 150;
	m_Fa18 = 250;
	m_Federal = 210;
	m_Spa = 160;
	m_Ben = 120;
	m_Sw = 50;
	m_Nm33 = 150;
	m_Mini = 100;
}

void CLimitator::Spawn( void )
{
	pev->solid = SOLID_NOT;
	Limitat = 1;

	// 0 or below means no reserve ammo; above 900, the most each weapon carries, gives 900.
	int *values[11] = { &m_Rc28, &m_K444, &m_Canon, &m_Pm66, &m_Fa18, &m_Federal, &m_Spa, &m_Ben, &m_Sw, &m_Nm33,
		&m_Mini };
	for ( int i = 0; i < 11; i++ )
	{
		int &v = *values[i];
		if ( v < 0 )
			v = 0;
		else if ( v > 900 )
			v = 900;
	}
}

void CLimitator::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "weap1" ) )
	{
		m_Rc28 = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "weap2" ) )
	{
		m_K444 = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "weap3" ) )
	{
		m_Canon = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "weap4" ) )
	{
		m_Pm66 = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "weap5" ) )
	{
		m_Fa18 = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "weap6" ) )
	{
		m_Federal = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "weap7" ) )
	{
		m_Spa = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "weap8" ) )
	{
		m_Ben = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "weap9" ) )
	{
		m_Sw = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "weap10" ) )
	{
		m_Nm33 = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "weap11" ) )
	{
		m_Mini = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}

}
