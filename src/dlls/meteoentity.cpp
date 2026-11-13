//
// meteoentity.cpp
//
// SBO: meteo_particules, the rain and snow entity of SBO 1.38. It was removed in SBO 2.0 and is not in the FGD; new
// maps use env_rain / env_snow, and the shipped maps that had it carry env_rain / env_snow themselves. A map that still
// has one gets, at spawn, an unnamed env_snow (iType 0 or missing) or env_rain (iType != 0) at its origin with no keys
// (always on, CS's density; triggers.cpp CWeather), a developer console message (developer 1) says so, and the entity
// removes itself. Its keys iParticules, iSize, iVitesse and iHauteur are ignored; the client's cl_weather sets the
// intensity. It plays no sound: a rain sound is the map's own ambient_generic. See docs/WEATHER.md.
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"

class CMeteoEntity : public CBaseEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );

	int m_iType;	// 0 snow, else rain
};

LINK_ENTITY_TO_CLASS( meteo_particules, CMeteoEntity );

void CMeteoEntity::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "iType"))
	{
		m_iType = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "iParticules") || FStrEq(pkvd->szKeyName, "iSize")
		|| FStrEq(pkvd->szKeyName, "iVitesse") || FStrEq(pkvd->szKeyName, "iHauteur"))
	{
		pkvd->fHandled = TRUE;	// 1.38 tuning, no CS equivalent
	}
	else
	{
		CBaseEntity::KeyValue( pkvd );
	}
}

void CMeteoEntity::Spawn( void )
{
	// an unnamed point env_snow / env_rain (triggers.cpp CWeather) in its place
	const char *pszWeather = m_iType ? "env_rain" : "env_snow";
	ALERT( at_console, "meteo_particules at (%.0f %.0f %.0f) was removed in SBO 2.0; using %s instead\n",
		pev->origin.x, pev->origin.y, pev->origin.z, pszWeather );
	CBaseEntity::Create( (char *)pszWeather, pev->origin, pev->angles );
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
	UTIL_Remove( this );
}
