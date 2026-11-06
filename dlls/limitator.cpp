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

void CLimitator::Spawn( void )
{
	pev->solid = SOLID_NOT;
	Limitat = 1;
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

	if ( m_Rc28 > 910 || m_Rc28 < 1 )
		m_Rc28 = 1;
	if ( m_K444 > 910 || m_K444 < 1 )
		m_K444 = 1;
	if ( m_Canon > 910 || m_Canon < 1 )
		m_Canon = 1;
	if ( m_Pm66 > 910 || m_Pm66 < 1 )
		m_Pm66 = 1;
	if ( m_Fa18 > 910 || m_Fa18 < 1 )
		m_Fa18 = 1;
	if ( m_Federal > 910 || m_Federal < 1 )
		m_Federal = 1;
	if ( m_Spa > 910 || m_Spa < 1 )
		m_Spa = 1;
	if ( m_Ben > 910 || m_Ben < 1 )
		m_Ben = 1;
	if ( m_Sw > 910 || m_Sw < 1 )
		m_Sw = 1;
	if ( m_Nm33 > 910 || m_Nm33 < 1 )
		m_Nm33 = 1;
	if ( m_Mini > 910 || m_Mini < 1 )
		m_Mini = 1;
}
