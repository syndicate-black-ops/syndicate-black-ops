//
// meteoentity.cpp
//
// SBO: weather entity of 1.38 (meteo_particules), kept so a custom map made for 1.38 still gets its weather.
//
// SBO 2.0: 1.38 drew it with its own client emitter (FallingPart message, CFallingPart); it now selects the CS 1.6
// weather (env_rain / env_snow, ReGameDLL_CS CWeather + cs16-client environment.cpp): at spawn the entity renames
// itself to env_snow (iType 0 or missing) or env_rain (iType != 0), so CBasePlayer::SendWeatherInfo finds it. The
// 1.38 tuning keys iParticules, iSize, iVitesse (speed) and iHauteur (height) have no CS equivalent and are ignored;
// the client's cl_weather sets the intensity. The entity is not in the FGD: new maps use env_rain / env_snow, and
// the shipped maps that had it (bk_mongolito, br_mongolito, co_carnage, co_kill-the-doc) carry env_rain / env_snow
// themselves. It plays no sound; a rain sound is the map's own ambient_generic. See docs/WEATHER.md.
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
	// becomes a point env_snow / env_rain (see CWeather::Spawn, triggers.cpp)
	const char *pszWeather = m_iType ? "env_rain" : "env_snow";
	pev->classname = MAKE_STRING( pszWeather );
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
}
