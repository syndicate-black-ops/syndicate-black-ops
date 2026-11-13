// environment.cpp -- Counter-Strike 1.6 weather (rain, snow) on the client.
//
// Ported from cs16-client cl_dll/environment.cpp (https://github.com/Velaron/cs16-client, c1604b6), the CS 1.6 path:
// the server's ReceiveW message (0 none, 1 rain, 2 snow) starts a global weather around the view, drawn with the
// particle manager (particleman/). Rain: drops, splashes on solid ground, ripples on water, wind-blown dust puffs.
// Snow: spiralling flakes that settle on surfaces and dissolve in water. Wind changes direction every 15-30 s.
// Particles only spawn under the sky (the trace up from the spawn point must hit a "sky" texture; SBO 2.0: or the sky
// volume of the env_sky tool texture, SkyAbove).
//
// SBO changes (all marked "SBO"):
//  * left out cs16-client's Rain/Snow/WeatherPos messages and gl_snow_* cvars (not CS);
//  * sprites come from one table (WEATHER_SPRITES): CS's own sprites and render modes, shipped with SBO; a missing or
//    broken sprite disables that part instead of crashing;
//  * weather origin = the rendered view origin (v_origin; spectator modes, chase cam) instead of player origin + 36;
//  * no weather on the spectator's overview map; with a camera inset it is around the inset's camera (tri.cpp);
//  * particles can be refused at the particle cap (CMiniMem::MaxParticles): every `new` is checked;
//  * ReceiveW replaces the current weather (no duplicate warnings when it is resent on each respawn).
//  * SBO 2.0: env_rain / env_snow keys and the rain / snow volumes arrive in WeatherVol (MsgFunc_WeatherVol), which
//    sets the RainData / SnowData fields cs16-client's Rain / Snow messages set.
//  * SBO 2.0: a weather's own wind (Spirit of Half-Life's rain_modify wind: a fixed sideways speed and a random
//    spread per drop / flake, cl_dll/rain.cpp), which replaces the CS wind for that weather (WeatherData::WindVelocity).
//  * a ceiling of the env_sky tool texture (SoHL 3D sky, no face) counts as sky (SkyAbove).
// See docs/WEATHER.md.
//
// CS16Client: Copyright (C) 2015-2016 Flying With Gauss. This file is free software under the GNU General Public
// License v2 or later, with the special exception that permits linking with the Half-Life engine and Valve's MODs
// (see docs/WEATHER.md, "Licence").

#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "event_api.h"
#include "const.h"
#include "com_model.h"

#include "environment.h"

#include "particleman.h"
#include "r_studioint.h"
#include "triangleapi.h"

#include "pm_defs.h"
#include "pmtrace.h"
#include "pm_materials.h"

extern engine_studio_api_t IEngineStudio;

extern Vector g_vPlayerVelocity;	// hl_weapons.cpp, HUD_PostRunCmd
extern vec3_t v_origin;				// view.cpp, origin of the view being rendered
extern const Vector g_vecZero;

void VectorAngles( const float *forward, float *angles );
void HUD_GetEffectAngles( float *angles );	// tri.cpp

// SBO 2.0: the trace up from a weather particle reached the sky: a "sky" face, or the sky volume of the env_sky tool
// texture, which has no face to name (SoHL env_sky, docs/ENV_SKY.md). EV_SetTraceHull( large_hull ) is the point hull
// (pmove's usehull 2 is BSP hull 0, see PM_HullForBsp), and a trace passes through sky contents: it stops where the
// solid outside of the map begins, at the far side of the sky brush. Just short of that point is the sky volume.
static bool SkyAbove( const pmtrace_t &trace, const char *pszTexture )
{
	if ( pszTexture && strncmp( pszTexture, "sky", 3 ) == 0 )
		return true;

	if ( trace.fraction >= 1.0f || trace.allsolid || trace.ent != 0 )
		return false;

	// the traces go up: one unit back down from where it stopped (in case the hull is the 64-unit one after all,
	// whose clip hull is solid at the sky brush: its centre then stops 32 units below it)
	Vector vecInside = trace.endpos;
	vecInside.z -= 1.0f;

	if ( gEngfuncs.PM_PointContents( vecInside, nullptr ) == CONTENTS_SKY )
		return true;

	vecInside.z += 34.0f;

	return gEngfuncs.PM_PointContents( vecInside, nullptr ) == CONTENTS_SKY;
}

// cs16-client game_shared/pi_constant.h
#if !defined(M_PI)
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

// cs16-client game_shared/hull_types.h
enum
{
	point_hull = 0,
	human_hull = 1,
	large_hull = 2,
	head_hull = 3
};

// cs16-client game_shared/fx_flags.h (weather part). The CS path only uses SF_RAIN_ACTIVE / SF_SNOW_ACTIVE; the other
// flags select behaviour of the WeatherData helpers kept from the reference.
#define SF_WEATHER_ACTIVE (1 << 0)
#define SF_WEATHER_ALLOW_INDOOR (1 << 1)
#define SF_WEATHER_NOT_AFFECTED_BY_WIND (1 << 2)
#define SF_WEATHER_LOCALIZED (1 << 3)
#define SF_WEATHER_DISTANCE_IS_RADIUS (1 << 4)
#define SF_WEATHER_GO_THROUGH_BRUSH_ENTITIES (1 << 8)
#define SF_WEATHER_IGNORE_WATER (1 << 9)
#define WEATHER_BRUSH_ENTITY (1 << 12)
#define WEATHER_FIXED_WIND (1 << 13)	// SBO 2.0: the server's wind_x / wind_y (triggers.cpp WEATHERVOL_FIXED_WIND)

#define SF_RAIN_ACTIVE SF_WEATHER_ACTIVE
#define SF_RAIN_NOT_AFFECTED_BY_WIND SF_WEATHER_NOT_AFFECTED_BY_WIND
#define SF_RAIN_NO_WIND (1 << 5)
#define SF_RAIN_NO_SPLASHES (1 << 6)
#define SF_RAIN_NO_RIPPLES (1 << 7)

#define SF_SNOW_ACTIVE SF_WEATHER_ACTIVE
#define SF_SNOW_NOT_AFFECTED_BY_WIND SF_WEATHER_NOT_AFFECTED_BY_WIND

static inline float Com_RandomFloat( float flLow, float flHigh )
{
	return gEngfuncs.pfnRandomFloat( flLow, flHigh );
}

static inline int Com_RandomLong( int lLow, int lHigh )
{
	return gEngfuncs.pfnRandomLong( lLow, lHigh );
}

static float UTIL_AngleMod( float a )
{
	a = fmod( a, 360.0f );
	if( a < 0 )
		a += 360;
	return a;
}

static float UTIL_ApproachAngle( float target, float value, float speed )
{
	target = UTIL_AngleMod( target );
	value = UTIL_AngleMod( value );

	float delta = target - value;

	if( speed < 0 )
		speed = -speed;

	if( delta < -180 )
		delta += 360;
	else if( delta > 180 )
		delta -= 360;

	if( delta > speed )
		value += speed;
	else if( delta < -speed )
		value -= speed;
	else
		value = target;

	return value;
}

// SBO: the weather sprites, in one place: exactly cs16-client's / CS 1.6's paths and render modes (cs16-client's
// *_DEFAULT_SPRITE constants and the RainData / SnowData constructors). The files are Counter-Strike's own, shipped in
// SBO's gamedir at the same paths; wsplash3.spr comes from base Half-Life (valve/sprites/). docs/WEATHER.md, "Assets".
struct WeatherSprite
{
	const char* path;
	int renderMode;
};

enum
{
	WEATHER_SPRITE_RAINDROP = 0,
	WEATHER_SPRITE_WINDPUFF,
	WEATHER_SPRITE_SPLASH,
	WEATHER_SPRITE_RIPPLE,
	WEATHER_SPRITE_SNOWFLAKE,
	WEATHER_SPRITE_COUNT
};

static const WeatherSprite WEATHER_SPRITES[WEATHER_SPRITE_COUNT] =
{
	{ "sprites/effects/rain.spr", kRenderTransAlpha },		// raindrop
	{ "sprites/gas_puff_01.spr", kRenderTransAlpha },		// wind puff
	{ "sprites/wsplash3.spr", kRenderTransAdd },			// splash (base Half-Life)
	{ "sprites/effects/ripple.spr", kRenderTransAdd },		// ripple
	{ "sprites/effects/snowflake.spr", kRenderTransAdd },	// snowflake
};

constexpr int DEFAULT_WEATHER_INTENSITY = 150;
constexpr float DEFAULT_WEATHER_UPDATE_PERIOD = 0.3f;
constexpr int DEFAULT_WEATHER_DISTANCE = 400;
constexpr int DEFAULT_PRECIPITATION_MIN_HEIGHT = 100;
constexpr int DEFAULT_PRECIPITATION_MAX_HEIGHT = 300;

CEnvironment g_Environment;

static Vector FindSurfaceDividerPoint(const Vector& vecStart, const Vector& vecFinish)
{
	Vector vecBegin = vecStart;
	Vector vecEnd = vecFinish;

	Vector vecDist = vecEnd - vecBegin;

	// SBO: bounded (a bisection needs ~log2(length / 4) steps; 32 covers any map) so bad input cannot hang the client
	for (int iStep = 0; iStep < 32 && vecDist.Length() > 4.0f; ++iStep)
	{
		vecDist = vecDist * 0.5f;

		Vector vecHalf = vecBegin + vecDist;

		if (gEngfuncs.PM_PointContents( vecBegin, nullptr ) == gEngfuncs.PM_PointContents( vecHalf, nullptr ))
		{
			vecBegin = vecHalf;
		}
		else
		{
			vecEnd = vecHalf;
		}
		vecDist = vecEnd - vecBegin;
	}

	return vecBegin;
}

float ParticleParams::GetSize() const
{
	if (maxSize > minSize)
		return Com_RandomFloat(minSize, maxSize);
	return minSize;
}

WeatherData::WeatherData():
	entIndex(0),
	flags(0),
	intensity(DEFAULT_WEATHER_INTENSITY),
	updatePeriod(DEFAULT_WEATHER_UPDATE_PERIOD),
	distance(DEFAULT_WEATHER_DISTANCE),
	minHeight(DEFAULT_PRECIPITATION_MIN_HEIGHT),
	maxHeight(DEFAULT_PRECIPITATION_MAX_HEIGHT),
	location(0.0f,0.0f,0.0f),
	absmin(0.0f,0.0f,0.0f),
	absmax(0.0f,0.0f,0.0f),
	updateTime(0.0f),
	windX(0.0f),
	windY(0.0f),
	windRandX(0.0f),
	windRandY(0.0f)
{}

bool WeatherData::HasFixedWind() const
{
	return (flags & WEATHER_FIXED_WIND) != 0;
}

// SBO 2.0: Spirit's random spread on the sideways speed of a new drop / flake (-randX..randX, -randY..randY), on the
// fixed wind or the CS wind. No spread: no extra random calls.
void WeatherData::AddWindSpread(Vector& velocity) const
{
	if (windRandX > 0.0f)
		velocity.x += Com_RandomFloat(-windRandX, windRandX);
	if (windRandY > 0.0f)
		velocity.y += Com_RandomFloat(-windRandY, windRandY);
}

bool WeatherData::IsLocalizedPoint() const
{
	return (flags & SF_WEATHER_LOCALIZED) != 0;
}

bool WeatherData::IsVolume() const
{
	return (flags & WEATHER_BRUSH_ENTITY) != 0;
}

bool WeatherData::IsGlobal() const
{
	return !IsLocalizedPoint() && !IsVolume();
}

bool WeatherData::AllowIndoors() const
{
	return (flags & SF_WEATHER_ALLOW_INDOOR) != 0;
}

bool WeatherData::DistanceIsRadius() const
{
	return (flags & SF_WEATHER_DISTANCE_IS_RADIUS) != 0;
}

bool WeatherData::NeedsPVSCheck() const
{
	return IsLocalizedPoint() || IsVolume();
}

float WeatherData::RandomHeight() const
{
	if (maxHeight > minHeight)
		return Com_RandomFloat(minHeight, maxHeight);
	return minHeight;
}

bool WeatherData::CanGoThroughBrushEntities() const
{
	return (flags & SF_WEATHER_GO_THROUGH_BRUSH_ENTITIES) != 0;
}

bool WeatherData::ShouldTraceWater() const
{
	return (flags & SF_WEATHER_IGNORE_WATER) == 0;
}

Vector WeatherData::GetWeatherOrigin(const Vector &globalWeatherOrigin) const
{
	if (IsVolume())
	{
		Vector result = (absmin + absmax)/2.0f;
		result.z = absmin.z + 8.0f;
		return result;
	}
	if (IsLocalizedPoint())
		return location;
	return globalWeatherOrigin;
}

void WeatherData::GetBoundingBox(const Vector &weatherOrigin, Vector &mins, Vector &maxs) const
{
	if (IsVolume())
	{
		mins = absmin;
		maxs = absmax;
	}
	else
	{
		mins = weatherOrigin - Vector(distance, distance, 0);
		maxs = weatherOrigin + Vector(distance, distance, (std::max)(minHeight, maxHeight));
	}
}

Vector WeatherData::GetRandomOrigin(const Vector& weatherOrigin) const
{
	Vector vecOrigin;
	if (IsVolume())
	{
		vecOrigin.x = Com_RandomFloat(absmin.x, absmax.x);
		vecOrigin.y = Com_RandomFloat(absmin.y, absmax.y);
		const float minZ = absmin.z + (absmax.z - absmin.z) / 3.0f;
		vecOrigin.z = Com_RandomFloat(minZ, absmax.z);
	}
	else
	{
		vecOrigin = weatherOrigin;
		if (DistanceIsRadius())
		{
			const float radius = Com_RandomFloat( -distance, distance );
			const float angle = Com_RandomFloat(0.0f, M_PI * 2);
			vecOrigin.x += radius * cos(angle);
			vecOrigin.y += radius * sin(angle);
		}
		else
		{
			vecOrigin.x += Com_RandomFloat( -distance, distance );
			vecOrigin.y += Com_RandomFloat( -distance, distance );
		}
		vecOrigin.z += RandomHeight();
	}
	return vecOrigin;
}

RainData::RainData():
	WeatherData(),
	rainSprite(nullptr),
	windPuffSprite(nullptr),
	splashSprite(nullptr),
	rippleSprite(nullptr)
{
	raindropParticleParams.minSize = raindropParticleParams.maxSize = 2.0f;
	raindropParticleParams.color = Vector(255.0f, 255.0f, 255.0f);
	raindropParticleParams.brightness = 128;
	raindropParticleParams.renderMode = WEATHER_SPRITES[WEATHER_SPRITE_RAINDROP].renderMode;
	raindropParticleParams.lightFlag = LIGHT_NONE;
	raindropStretchY = 40.0f;

	raindropMinSpeed = 500.0f;
	raindropMaxSpeed = 1800.0f;
	raindropLife = 1.0f;

	windParticleParams.minSize = 50.0f;
	windParticleParams.maxSize = 75.0f;
	windParticleParams.color = Vector(128.0f, 128.0f, 128.0f);
	windParticleParams.brightness = 128;
	windParticleParams.renderMode = WEATHER_SPRITES[WEATHER_SPRITE_WINDPUFF].renderMode;
	windParticleParams.lightFlag = LIGHT_NONE;

	windpuffLife = 6.0f;

	splashParticleParams.minSize = 20;
	splashParticleParams.maxSize = 25;
	splashParticleParams.color = Vector(255.0f, 255.0f, 255.0f);
	splashParticleParams.brightness = 150;
	splashParticleParams.renderMode = WEATHER_SPRITES[WEATHER_SPRITE_SPLASH].renderMode;
	splashParticleParams.lightFlag = LIGHT_INTENSITY;

	rippleParticleParams.minSize = rippleParticleParams.maxSize = 15.0f;
	rippleParticleParams.color = Vector(255.0f, 255.0f, 255.0f);
	rippleParticleParams.brightness = 150;
	rippleParticleParams.renderMode = WEATHER_SPRITES[WEATHER_SPRITE_RIPPLE].renderMode;
	rippleParticleParams.lightFlag = LIGHT_INTENSITY;
}

float RainData::GetRaindropFallingSpeed() const
{
	float speed1 = fabs(raindropMinSpeed);
	float speed2 = fabs(raindropMaxSpeed);

	float minSpeed = speed1 < speed2 ? speed1 : speed2;
	float maxSpeed = speed1 > speed2 ? speed1 : speed2;

	if (maxSpeed > minSpeed)
		return Com_RandomFloat(minSpeed, maxSpeed);
	return minSpeed;
}

bool RainData::RaindropsAffectedByWind() const
{
	return !(flags & SF_RAIN_NOT_AFFECTED_BY_WIND);
}

bool RainData::WindPuffsAllowed() const
{
	return !(flags & SF_RAIN_NO_WIND);
}

bool RainData::SplashesAllowed() const
{
	return !(flags & SF_RAIN_NO_SPLASHES);
}

bool RainData::RipplesAllowed() const
{
	return !(flags & SF_RAIN_NO_RIPPLES);
}

SnowData::SnowData():
	WeatherData(),
	snowSprite(nullptr)
{
	updatePeriod = 0.5f;
	distance = 1024;
	intensity = 512;

	snowflakeParticleParams.minSize = 2.1f;
	snowflakeParticleParams.maxSize = 2.6f;
	snowflakeParticleParams.color = Vector(200.0f, 200.0f, 200.0f);
	snowflakeParticleParams.brightness = 154.0f;
	snowflakeParticleParams.renderMode = WEATHER_SPRITES[WEATHER_SPRITE_SNOWFLAKE].renderMode;
	snowflakeParticleParams.lightFlag = LIGHT_NONE;
	snowflakeInitialBrightness = 154;
	snowflakeMinSpeed = 131;
	snowflakeMaxSpeed = 210;
	snowflakeLife = 3.0f;
}

float SnowData::GetSnowflakeFallingSpeed() const
{
	float speed1 = fabs(snowflakeMinSpeed);
	float speed2 = fabs(snowflakeMaxSpeed);

	float minSpeed = speed1 < speed2 ? speed1 : speed2;
	float maxSpeed = speed1 > speed2 ? speed1 : speed2;

	if (maxSpeed > minSpeed)
		return Com_RandomFloat(minSpeed, maxSpeed);
	return minSpeed;
}

bool SnowData::SnowflakesAffectedByWind() const
{
	return !(flags & SF_SNOW_NOT_AFFECTED_BY_WIND);
}

class CPartRainDrop : public CBaseParticle
{
public:
	CPartRainDrop() = default;
	void Think( float flTime ) override;
	void Touch(Vector pos, Vector normal, int index, bool enteringWater) override;

	bool m_splashAllowed = true;
	bool m_rippleAllowed = true;
	ParticleParams m_splashParams;
	ParticleParams m_rippleParams;
	model_t* m_pRainSplash = nullptr;
	model_t* m_pRipple = nullptr;

private:
	bool m_bTouched = false;
	bool m_playedSplash = false;
	bool m_playedRipple = false;
};

void CPartRainDrop::Think( float flTime )
{
	Vector vecViewAngles;
	Vector vecForward, vecRight, vecUp;

	HUD_GetEffectAngles( vecViewAngles );	// SBO 2.0: the view being drawn (the spectator's camera inset has its own)

	AngleVectors( vecViewAngles, vecForward, vecRight, vecUp );

	m_vAngles.y = vecViewAngles.y;
	m_vAngles.z = atan( DotProduct( m_vVelocity, vecRight ) / m_vVelocity.z ) * ( 180.0 / M_PI );

	if( m_flBrightness < 155.0f )
		m_flBrightness += 6.5f;

	CBaseParticle::Think( flTime );
}

void CPartRainDrop::Touch(Vector pos, Vector normal, int index, bool enteringWater)
{
	if (m_bTouched)
	{
		return;
	}
	if (m_playedSplash && m_playedRipple)
	{
		return;
	}

	if (enteringWater)
	{
		if ((GetCollisionFlags() & TRI_WATERTRACEKILL) != 0)
		{
			m_bTouched = true;
		}
	}
	else
	{
		m_bTouched = true;
	}

	Vector vecStart = m_vOrigin;
	vecStart.z += 32.0f;

	pmtrace_t trace;

	{
		int traceFlags = PM_STUDIO_BOX | PM_STUDIO_IGNORE;
		if ((GetCollisionFlags() & (TRI_COLLIDEBRUSHENTS)) == 0)
			traceFlags = PM_WORLD_ONLY;

		Vector vecEnd = m_vOrigin;
		vecEnd.z -= 16.0f;

		gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, traceFlags, -1, &trace);

		if (trace.ent != 0 && (GetCollisionFlags() & (TRI_COLLIDEBRUSHENTS)) != 0)
		{
			physent_t* entity = gEngfuncs.pEventAPI->EV_GetPhysent(trace.ent);
			if (!entity || !entity->model || entity->model->name[0] != '*')
			{
				gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_WORLD_ONLY, -1, &trace);
			}
		}
	}

	Vector vecNormal;

	vecNormal.x = normal.x;
	vecNormal.y = normal.y;
	vecNormal.z = -normal.z;

	Vector vecAngles;

	VectorAngles( vecNormal, vecAngles );

	if (gEngfuncs.PM_PointContents( trace.endpos, nullptr ) == gEngfuncs.PM_PointContents( vecStart, nullptr ))
	{
		if (!m_playedSplash && m_splashAllowed && m_pRainSplash)
		{
			CBaseParticle* pParticle = new CBaseParticle();

			// SBO: nullptr at the particle cap
			if (pParticle)
			{
				model_t* pRainSplash = m_pRainSplash;

				pParticle->InitializeSprite( m_vOrigin + normal, Vector( 90.0f, 0.0f, 0.0f ), pRainSplash, m_splashParams.GetSize(), m_splashParams.brightness );

				pParticle->m_iRendermode = m_splashParams.renderMode;

				pParticle->m_flMass = 1.0f;
				pParticle->m_flGravity = 0.1f;

				pParticle->SetCullFlag( CULL_PVS );
				pParticle->SetLightFlag( m_splashParams.lightFlag );

				pParticle->m_vColor = m_splashParams.color;

				pParticle->m_iNumFrames = pRainSplash->numframes - 1;
				pParticle->m_iFramerate = Com_RandomLong( 30, 45 );
				pParticle->m_flDieTime = gEngfuncs.GetClientTime() + 0.3f;
				pParticle->SetCollisionFlags( TRI_ANIMATEDIE );
				pParticle->SetRenderFlag( RENDER_FACEPLAYER );
			}
		}
		m_playedSplash = true;
	}
	else
	{
		if (!m_playedRipple && m_rippleAllowed && m_pRipple)
		{
			Vector vecBegin = FindSurfaceDividerPoint(vecStart, trace.endpos);

			CBaseParticle* pParticle = new CBaseParticle();

			// SBO: nullptr at the particle cap
			if (pParticle)
			{
				pParticle->InitializeSprite( vecBegin, vecAngles, m_pRipple, m_rippleParams.GetSize(), m_rippleParams.brightness );

				pParticle->m_iRendermode = m_rippleParams.renderMode;
				pParticle->m_flScaleSpeed = 1.0f;
				pParticle->m_vColor = m_rippleParams.color;
				pParticle->SetCullFlag( CULL_PVS );
				pParticle->SetLightFlag( m_rippleParams.lightFlag );
				pParticle->m_flFadeSpeed = 2.0f;
				pParticle->m_flDieTime = gEngfuncs.GetClientTime() + 2.0f;
			}
		}
		m_playedRipple = true;
	}
}

class CPartWind : public CBaseParticle
{
public:
	CPartWind() = default;

	void Think( float flTime ) override;
};

void CPartWind::Think( float flTime )
{
	if( m_flDieTime - flTime <= 3.0 )
	{
		if( m_flBrightness > 0.0 )
		{
			m_flBrightness -= ( flTime - m_flTimeCreated ) * 0.4;
		}

		if( m_flBrightness < 0.0 )
		{
			m_flBrightness = 0;
			flTime = m_flDieTime = gEngfuncs.GetClientTime();
		}
	}
	else
	{
		if( m_flBrightness < 105.0 )
		{
			m_flBrightness += ( flTime - m_flTimeCreated ) * 5.0 + 4.0;
		}
	}

	CBaseParticle::Think( flTime );
}

class CPartSnowFlake : public CBaseParticle
{
public:
	CPartSnowFlake() = default;
	void Think( float flTime ) override;
	void Touch(Vector pos, Vector normal, int index, bool enteringWater) override;

public:
	bool m_bSpiral;
	float m_flSpiralTime;
	int m_targetBrightness;

private:
	bool m_bTouched = false;
	bool m_dissolving = false;
};

void CPartSnowFlake::Think( float flTime )
{
	if (m_flBrightness < m_targetBrightness && !m_bTouched && !m_dissolving)
		m_flBrightness += 4.5f;

	if (m_flBrightness > 255.0f)
		m_flBrightness = 255.0f;

	Fade( flTime );
	Spin( flTime );

	if( m_flSpiralTime <= gEngfuncs.GetClientTime() )
	{
		m_bSpiral = !m_bSpiral;

		m_flSpiralTime = gEngfuncs.GetClientTime() + Com_RandomLong( 2, 4 );
	}

	if( m_bSpiral && !m_bTouched )
	{
		const float flDelta = flTime - g_Environment.GetOldTime();

		const float flSpin = sin( flTime * 5.0 + reinterpret_cast<std::intptr_t>( this ) );

		m_vOrigin = m_vOrigin + m_vVelocity * flDelta;

		m_vOrigin.x += ( flSpin * flSpin ) * 0.3;
	}
	else
	{
		CalculateVelocity( flTime );
	}

	CheckCollision( flTime );
}

void CPartSnowFlake::Touch(Vector pos, Vector normal, int index, bool enteringWater)
{
	if( m_bTouched )
	{
		return;
	}

	if (enteringWater)
	{
		m_dissolving = true;

		m_flFadeSpeed = 0;

		m_flOriginalBrightness = m_flBrightness;
		m_flTimeCreated = gEngfuncs.GetClientTime();
		m_flDieTime = m_flTimeCreated + 0.5f;
	}
	else
	{
		m_bTouched = true;

		SetRenderFlag( RENDER_FACEPLAYER );

		m_vVelocity = g_vecZero;

		m_flFadeSpeed = 0;
		m_flScaleSpeed = 0;
		m_flDampingTime = 0;
		m_iFrame = 0;
		m_flMass = 1.0;
		m_flGravity = 0;

		if (!m_dissolving)
		{
			m_flOriginalBrightness = m_flBrightness;
			m_flTimeCreated = gEngfuncs.GetClientTime();
			m_flDieTime = m_flTimeCreated + 0.5f;
		}
	}
}

void CEnvironment::Initialize()
{
	Reset();

	m_flWeatherValue = gHUD.cl_weather ? gHUD.cl_weather->value : 0.0f;
	m_iSavedWeatherType = 0;
	m_savedRains.clear();
	m_savedSnows.clear();

	RestoreWeather();
}

void CEnvironment::RestoreWeather()
{
	if (m_iSavedWeatherType == 1)
	{
		RainData rainData;
		rainData.flags = SF_RAIN_ACTIVE;
		SetRainSprites(rainData);
		AddRain(rainData);
	}
	else if (m_iSavedWeatherType == 2)
	{
		SnowData snowData;
		snowData.flags = SF_SNOW_ACTIVE;
		SetSnowSprites(snowData);
		AddSnow(snowData);
	}

	// SBO 2.0: the server's env_rain / env_snow (WeatherVol); the sprites are loaded again (video restart)
	for (RainData rainData : m_savedRains)
	{
		SetRainSprites(rainData);
		AddRain(rainData);
	}
	for (SnowData snowData : m_savedSnows)
	{
		SetSnowSprites(snowData);
		AddSnow(snowData);
	}
}

void CEnvironment::SetRainSprites(RainData& rainData)
{
	if (!m_pRainSprite)
		m_pRainSprite = LoadSprite(WEATHER_SPRITES[WEATHER_SPRITE_RAINDROP].path);
	rainData.rainSprite = m_pRainSprite;

	if (!m_pGasPuffSprite)
		m_pGasPuffSprite = LoadSprite(WEATHER_SPRITES[WEATHER_SPRITE_WINDPUFF].path);
	rainData.windPuffSprite = m_pGasPuffSprite;

	if (!m_pRainSplash)
		m_pRainSplash = LoadSprite(WEATHER_SPRITES[WEATHER_SPRITE_SPLASH].path);
	rainData.splashSprite = m_pRainSplash;

	if (!m_pRipple)
		m_pRipple = LoadSprite(WEATHER_SPRITES[WEATHER_SPRITE_RIPPLE].path);
	rainData.rippleSprite = m_pRipple;
}

void CEnvironment::SetSnowSprites(SnowData& snowData)
{
	if (!m_pSnowSprite)
		m_pSnowSprite = LoadSprite(WEATHER_SPRITES[WEATHER_SPRITE_SNOWFLAKE].path);
	snowData.snowSprite = m_pSnowSprite;
}

void CEnvironment::Reset()
{
	m_pRainSprite = nullptr;
	m_pGasPuffSprite = nullptr;
	m_pRainSplash = nullptr;
	m_pRipple = nullptr;
	m_pSnowSprite = nullptr;

	// SBO 2.0: the grass pieces went with the particles; the next GrassInfo (InitHUD) starts it again
	ResetGrass();

	Clear();

	m_vecWeatherOrigin = g_vecZero;

	m_vecWind.x = Com_RandomFloat( -80.0f, 80.0f );
	m_vecWind.y = Com_RandomFloat( -80.0f, 80.0f );
	m_vecWind.z = 0;

	m_vecDesiredWindDirection.x = Com_RandomFloat( -80.0f, 80.0f );
	m_vecDesiredWindDirection.y = Com_RandomFloat( -80.0f, 80.0f );
	m_vecDesiredWindDirection.z = 0;

	m_flNextWindChangeTime = gEngfuncs.GetClientTime();
}

void CEnvironment::Clear()
{
	m_rains.clear();
	m_snows.clear();
}

void CEnvironment::Update()
{
	// SBO: the origin of the view being rendered (cs16-client: the local or spectated player's origin + 36)
	Vector vecOrigin = v_origin;

	if( gHUD.cl_weather && gHUD.cl_weather->value > 3.0 )
	{
		gEngfuncs.Cvar_SetValue( "cl_weather", 3.0 );
	}

	m_flWeatherValue = gHUD.cl_weather ? gHUD.cl_weather->value : 0.0f;

	if( !IEngineStudio.IsHardware || !IEngineStudio.IsHardware() )
		m_flWeatherValue = 0;

	m_vecWeatherOrigin = vecOrigin;

	if (ShouldUpdateWind())
		UpdateWind();

	const float clientTime = gEngfuncs.GetClientTime();

	for (auto& rain : m_rains)
	{
		if (rain.updateTime <= clientTime)
		{
			UpdateRain(rain);
			rain.updateTime = clientTime + rain.updatePeriod;
		}
	}
	for (auto& snow : m_snows)
	{
		if (snow.updateTime <= clientTime)
		{
			UpdateSnow(snow);
			snow.updateTime = clientTime + snow.updatePeriod;
		}
	}

	// SBO 2.0: grass
	UpdateGrass( vecOrigin );

	m_flOldTime = clientTime;
}

// ---------------------------------------------------------------------------------------------------------------
// SBO 2.0: grass (CS:CZ Deleted Scenes' CEnvironment::SetupGrass / UpdateGrass / PlaceGrass / CreateGrassPiece,
// behavior from static analysis of Valve's binaries, no code).
// The server sends the worldspawn keys (GrassInfo); grass grows only on textures of material A (grass) of the
// map's material data, so only on maps that opt in to SBO's tags or tag their own.
// ---------------------------------------------------------------------------------------------------------------

#define GRASS_MAX_PIECES	3000		// of CMiniMem::MaxParticles (16384), so weather and impacts keep room
#define GRASS_MAX_PER_PASS	1500

static int s_iGrassPieces = 0;

class CPartGrass : public CBaseParticle
{
public:
	CPartGrass() { s_iGrassPieces++; }
	~CPartGrass() override { s_iGrassPieces--; }

	void Think( float flTime ) override
	{
		// fades out towards the fade distance; removed well past it (the player walked away)
		const float flDist = ( m_vOrigin - Vector( v_origin ) ).Length2D();

		if ( flDist > m_flFadeDist + 200.0f )
		{
			m_flDieTime = flTime;
			return;
		}

		float flAlpha = ( m_flFadeDist - flDist ) / ( m_flFadeDist * 0.25f );
		flAlpha = flAlpha < 0.0f ? 0.0f : ( flAlpha > 1.0f ? 1.0f : flAlpha );
		m_flBrightness = 255.0f * flAlpha;

		CBaseParticle::Think( flTime );
	}

	float m_flFadeDist = 1000.0f;
};

int CEnvironment::MsgFunc_GrassInfo( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	strncpy( m_szGrassSprite, READ_STRING(), sizeof( m_szGrassSprite ) - 1 );
	m_szGrassSprite[ sizeof( m_szGrassSprite ) - 1 ] = '\0';
	m_flGrassFadeDist = READ_COORD();
	m_flGrassMinHeight = READ_COORD();
	m_flGrassMaxHeight = READ_COORD();
	const int iDensity = READ_LONG();

	ResetGrass();

	// bounds (the values come from a map): fade distance 128-4096, heights 4-256, density 1-1000; keys left out:
	// the FGD's defaults (800, 16, 32)
	if ( iDensity <= 0 )
		return 1;
	if ( m_flGrassFadeDist <= 0.0f )
		m_flGrassFadeDist = 800.0f;
	if ( m_flGrassMinHeight <= 0.0f )
		m_flGrassMinHeight = 16.0f;
	if ( m_flGrassMaxHeight <= 0.0f )
		m_flGrassMaxHeight = 32.0f;
	m_flGrassFadeDist = m_flGrassFadeDist < 128.0f ? 128.0f : ( m_flGrassFadeDist > 4096.0f ? 4096.0f : m_flGrassFadeDist );
	m_flGrassMinHeight = m_flGrassMinHeight < 4.0f ? 4.0f : ( m_flGrassMinHeight > 256.0f ? 256.0f : m_flGrassMinHeight );
	m_flGrassMaxHeight = m_flGrassMaxHeight > 256.0f ? 256.0f : m_flGrassMaxHeight;
	if ( m_flGrassMaxHeight < m_flGrassMinHeight )
		m_flGrassMaxHeight = m_flGrassMinHeight;
	m_flGrassDensity = iDensity > 1000 ? 1000.0f : (float)iDensity;

	if ( !MapHasGrassTextures() )
		return 1;

	// an empty sprite: CS:CZ DS's pair
	if ( m_szGrassSprite[ 0 ] && MAT_IsSafeAssetPath( m_szGrassSprite, ".spr" ) )
	{
		m_pGrassSprite1 = m_pGrassSprite2 = Materials_Sprite( m_szGrassSprite );
	}
	else
	{
		m_pGrassSprite1 = Materials_Sprite( "sprites/grass_01.spr" );
		m_pGrassSprite2 = Materials_Sprite( "sprites/grass_03.spr" );
	}
	if ( !m_pGrassSprite1 && !m_pGrassSprite2 )
		return 1;

	m_bGrassActive = true;
	m_bGrassPlaceInitial = true;
	return 1;
}

void CEnvironment::ResetGrass()
{
	m_bGrassActive = false;
	m_bGrassPlaceInitial = false;
	m_flGrassNextUpdate = 0.0f;
	m_pGrassSprite1 = m_pGrassSprite2 = nullptr;
}

// any texture of the world model with material A
bool CEnvironment::MapHasGrassTextures() const
{
	cl_entity_t *world = gEngfuncs.GetEntityByIndex( 0 );
	model_t *model = world ? world->model : nullptr;

	if ( !model || model->type != mod_brush || !model->textures || model->numtextures <= 0 || model->numtextures > 4096 )
		return false;

	for ( int i = 0; i < model->numtextures; i++ )
	{
		const texture_t *texture = model->textures[ i ];
		char name[ CBTEXTURENAMEMAX ];
		char raw[ 17 ];

		if ( !texture )
			continue;
		memcpy( raw, texture->name, 16 );
		raw[ 16 ] = '\0';
		MAT_StripTextureName( raw, name );
		if ( MAT_FindTextureType( name ) == CHAR_TEX_GRASS )
			return true;
	}
	return false;
}

void CEnvironment::PlaceGrass( const Vector& vecOrigin, float innerRadius, float outerRadius, int quantity )
{
	if ( quantity > GRASS_MAX_PER_PASS )
		quantity = GRASS_MAX_PER_PASS;

	for ( int i = 0; i < quantity && s_iGrassPieces < GRASS_MAX_PIECES; i++ )
	{
		// a random point of the square, outside the inner square
		Vector vecStart = vecOrigin;
		vecStart.x += gEngfuncs.pfnRandomFloat( -outerRadius, outerRadius );
		vecStart.y += gEngfuncs.pfnRandomFloat( -outerRadius, outerRadius );
		if ( innerRadius > 0.0f && fabs( vecStart.x - vecOrigin.x ) < innerRadius && fabs( vecStart.y - vecOrigin.y ) < innerRadius )
			continue;
		vecStart.z += 36.0f;

		Vector vecEnd = vecStart;
		vecEnd.z -= 800.0f;

		pmtrace_t trace;
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( vecStart, vecEnd, PM_WORLD_ONLY, -1, &trace );
		if ( trace.startsolid || trace.allsolid || trace.fraction >= 1.0f )
			continue;

		// must hit a grass texture
		Vector vecHitStart = Vector( trace.endpos ) + Vector( 0.0f, 0.0f, 4.0f );
		Vector vecHitEnd = Vector( trace.endpos ) - Vector( 0.0f, 0.0f, 4.0f );
		const char *pszTexture = gEngfuncs.pEventAPI->EV_TraceTexture( 0, vecHitStart, vecHitEnd );
		char name[ CBTEXTURENAMEMAX ];
		MAT_StripTextureName( pszTexture, name );
		if ( !pszTexture || MAT_FindTextureType( name ) != CHAR_TEX_GRASS )
			continue;

		model_t *pSprite = gEngfuncs.pfnRandomLong( 0, 1 ) ? m_pGrassSprite1 : m_pGrassSprite2;
		if ( !pSprite )
			pSprite = m_pGrassSprite1 ? m_pGrassSprite1 : m_pGrassSprite2;

		CPartGrass *pGrass = new CPartGrass();
		if ( !pGrass )		// the particle cap
			return;

		const float flHeight = gEngfuncs.pfnRandomFloat( m_flGrassMinHeight, m_flGrassMaxHeight );
		Vector vecPos = trace.endpos;
		vecPos.z += 10.0f;

		pGrass->InitializeSprite( vecPos, Vector( 0.0f, gEngfuncs.pfnRandomFloat( 0.0f, 359.0f ), 0.0f ), pSprite, flHeight, 255.0f );
		pGrass->m_flFadeDist = m_flGrassFadeDist;
		pGrass->m_iRendermode = kRenderTransAlpha;
		pGrass->SetLightFlag( LIGHT_COLOR );
		pGrass->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		pGrass->m_flDieTime = gEngfuncs.GetClientTime() + 99999.0f;
	}
}

void CEnvironment::UpdateGrass( const Vector& vecOrigin )
{
	if ( !m_bGrassActive || !IEngineStudio.IsHardware || !IEngineStudio.IsHardware() )
		return;

	const float flTime = gEngfuncs.GetClientTime();
	if ( flTime < m_flGrassNextUpdate )
		return;
	m_flGrassNextUpdate = flTime + 0.3f;

	if ( m_bGrassPlaceInitial )
	{
		// the first pass: the square of half-size fade distance + 50, then the density of a ring pass
		m_bGrassPlaceInitial = false;
		m_vecGrassOrigin = vecOrigin;
		PlaceGrass( vecOrigin, 0.0f, m_flGrassFadeDist + 50.0f, (int)( m_flGrassDensity * 1.273f ) );
		m_flGrassDensity *= ( m_flGrassFadeDist * 100.0f + 2500.0f ) / ( m_flGrassFadeDist * m_flGrassFadeDist );
		return;
	}

	// moved 50 units or more: the ring the player walks into
	if ( ( vecOrigin - m_vecGrassOrigin ).Length2D() < 50.0f )
		return;
	m_vecGrassOrigin = vecOrigin;
	PlaceGrass( vecOrigin, m_flGrassFadeDist, m_flGrassFadeDist + 50.0f, (int)( m_flGrassDensity + 0.5f ) );
}

bool CEnvironment::ShouldUpdateWind() const
{
	for (const auto& rain : m_rains)
	{
		if (rain.RaindropsAffectedByWind() || rain.WindPuffsAllowed())
			return true;
	}
	for(const auto& snow : m_snows)
	{
		if (snow.SnowflakesAffectedByWind())
			return true;
	}
	return false;
}

void CEnvironment::UpdateWind()
{
	if( m_flNextWindChangeTime <= gEngfuncs.GetClientTime() )
	{
		m_vecDesiredWindDirection.x = Com_RandomFloat( -80.0f, 80.0f );
		m_vecDesiredWindDirection.y = Com_RandomFloat( -80.0f, 80.0f );
		m_vecDesiredWindDirection.z = 0;

		m_flNextWindChangeTime = gEngfuncs.GetClientTime() + Com_RandomFloat( 15.0f, 30.0f );

		m_flDesiredWindSpeed = m_vecDesiredWindDirection.Length();
		Vector vecDir = m_vecDesiredWindDirection.Normalize();

		if( vecDir.x == 0.0f && vecDir.y == 0.0f )
		{
			m_flIdealYaw = 0;
		}
		else
		{
			m_flIdealYaw = floor( atan2( vecDir.y, vecDir.x ) * ( 180.0 / M_PI ) );

			if( m_flIdealYaw < 0.0f )
				m_flIdealYaw += 360.0f;
		}
	}

	Vector vecWindDir = m_vecWind;

	vecWindDir = vecWindDir.Normalize();

	Vector vecAngles;

	VectorAngles( vecWindDir, vecAngles );

	float flYaw;

	if( vecAngles.y < 0.0f )
	{
		flYaw = 120 * ( 3 * static_cast<int>( floor( vecAngles.y / 360.0 ) ) + 3 );
	}
	else
	{
		if( vecAngles.y < 360.0f )
		{
			flYaw = vecAngles.y;
		}
		else
		{
			flYaw = vecAngles.y - ( 360 * floor( vecAngles.y / 360.0 ) );
		}
	}

	if( m_flIdealYaw != flYaw )
	{
		const float flSpeed = ( gEngfuncs.GetClientTime() - m_flOldTime ) * 0.5 * 10.0;
		vecAngles.y = UTIL_ApproachAngle( m_flIdealYaw, flYaw, flSpeed );
	}

	Vector vecNewWind;

	AngleVectors( vecAngles, vecNewWind, nullptr, nullptr );

	m_vecWind = vecNewWind * m_flDesiredWindSpeed;
}

void CEnvironment::UpdateRain(const RainData& rainData)
{
	const float rainIntensity = rainData.intensity * m_flWeatherValue;
	if( rainIntensity > 0.0f )
	{
		int iWindParticle = 0;

		Vector vecEndPos;
		pmtrace_t trace;

		Vector weatherOrigin = rainData.GetWeatherOrigin(m_vecWeatherOrigin);

		int rainDropCount = 0;
		int windParticleCount = 0;

		const bool allowIndoors = rainData.AllowIndoors();
		const bool windParticlesEnabled = rainData.WindPuffsAllowed();

		bool inPvs = true;
		if (rainData.NeedsPVSCheck())
		{
			Vector pvsMins;
			Vector pvsMaxs;
			rainData.GetBoundingBox(weatherOrigin, pvsMins, pvsMaxs);
			inPvs = gEngfuncs.pTriAPI->BoxInPVS( pvsMins, pvsMaxs ) != 0;
		}

		// Optimization: don't create localized rain particles when not in PVS
		if (!inPvs)
			return;

		const int traceFlags = rainData.CanGoThroughBrushEntities() ? PM_WORLD_ONLY : (PM_STUDIO_BOX | PM_STUDIO_IGNORE);

		for( size_t uiIndex = 0; static_cast<float>( uiIndex ) < rainIntensity; ++uiIndex )
		{
			Vector vecOrigin = rainData.GetRandomOrigin(weatherOrigin);

			vecEndPos.x = vecOrigin.x + ( ( Com_RandomLong( 0, 5 ) > 2 ) ? g_vPlayerVelocity.x : -g_vPlayerVelocity.x );
			vecEndPos.y = vecOrigin.y + g_vPlayerVelocity.y;
			vecEndPos.z = 8000.0f;

			gEngfuncs.pEventAPI->EV_SetTraceHull( large_hull );
			gEngfuncs.pEventAPI->EV_PlayerTrace( vecOrigin, vecEndPos, traceFlags, -1, &trace );
			const char* pszTexture = gEngfuncs.pEventAPI->EV_TraceTexture( trace.ent, vecOrigin, trace.endpos );

			if( allowIndoors || SkyAbove( trace, pszTexture ) )
			{
				CPartRainDrop* rainParticle = CreateRaindrop( vecOrigin, rainData );
				if (rainParticle)
					rainDropCount++;

				if (windParticlesEnabled)
				{
					if( iWindParticle == 15 )
					{
						iWindParticle = 1;

						Vector vecWindOrigin;
						vecWindOrigin.x = vecOrigin.x;
						vecWindOrigin.y = vecOrigin.y;
						vecWindOrigin.z = weatherOrigin.z;

						vecEndPos.z = 8000.0f;

						gEngfuncs.pEventAPI->EV_SetTraceHull( large_hull );
						gEngfuncs.pEventAPI->EV_PlayerTrace( vecWindOrigin, vecEndPos, PM_WORLD_ONLY, -1, &trace );
						pszTexture = gEngfuncs.pEventAPI->EV_TraceTexture( trace.ent, vecOrigin, trace.endpos );

						if( allowIndoors || SkyAbove( trace, pszTexture ) )
						{
							vecEndPos.z = -8000.0f;

							gEngfuncs.pEventAPI->EV_SetTraceHull( large_hull );
							gEngfuncs.pEventAPI->EV_PlayerTrace( vecWindOrigin, vecEndPos, PM_WORLD_ONLY, -1, &trace );

							Vector vecWindPos = trace.endpos;
							if (gEngfuncs.PM_PointContents(vecWindOrigin, nullptr) != gEngfuncs.PM_PointContents(trace.endpos + Vector(0,0,1), nullptr))
							{
								vecWindPos = FindSurfaceDividerPoint(vecWindOrigin, trace.endpos);
							}

							CPartWind* windParticle = CreateWindParticle( vecWindPos, rainData );
							if (windParticle)
								windParticleCount++;
						}
					}
					else
					{
						++iWindParticle;
					}
				}
			}
		}

		//gEngfuncs.Con_Printf("Rain drops spawned: %d. Wind particles spawned: %d\n", rainDropCount, windParticleCount);
	}
}

void CEnvironment::UpdateSnow(const SnowData& snowData)
{
	const float snowIntensity = snowData.intensity * m_flWeatherValue;
	if( snowIntensity > 0.0f )
	{
		Vector vecEndPos;
		pmtrace_t trace;

		Vector weatherOrigin = snowData.GetWeatherOrigin(m_vecWeatherOrigin);

		const bool allowIndoors = snowData.AllowIndoors();

		bool inPvs = true;
		if (snowData.NeedsPVSCheck())
		{
			Vector pvsMins;
			Vector pvsMaxs;
			snowData.GetBoundingBox(weatherOrigin, pvsMins, pvsMaxs);
			inPvs = gEngfuncs.pTriAPI->BoxInPVS( pvsMins, pvsMaxs ) != 0;
		}

		// Optimization: don't create localized snow particles when not in PVS
		if (!inPvs)
			return;

		const int traceFlags = snowData.CanGoThroughBrushEntities() ? PM_WORLD_ONLY : (PM_STUDIO_BOX | PM_STUDIO_IGNORE);

		for( size_t uiIndex = 0; static_cast<float>( uiIndex ) < snowIntensity; ++uiIndex )
		{
			Vector vecOrigin = snowData.GetRandomOrigin(weatherOrigin);

			vecEndPos.x = vecOrigin.x + ( ( Com_RandomLong( 0, 5 ) > 2 ) ? g_vPlayerVelocity.x : -g_vPlayerVelocity.x );
			vecEndPos.y = vecOrigin.y + g_vPlayerVelocity.y;
			vecEndPos.z = 8000.0f;

			gEngfuncs.pEventAPI->EV_SetTraceHull( large_hull );
			gEngfuncs.pEventAPI->EV_PlayerTrace( vecOrigin, vecEndPos, traceFlags, -1, &trace );
			const char* pszTexture = gEngfuncs.pEventAPI->EV_TraceTexture( trace.ent, vecOrigin, trace.endpos );

			if( allowIndoors || SkyAbove( trace, pszTexture ) )
			{
				CreateSnowFlake( vecOrigin, snowData );
			}
		}
	}
}

CPartRainDrop* CEnvironment::CreateRaindrop( const Vector& vecOrigin, const RainData& rainData )
{
	if( !rainData.rainSprite )
	{
		return nullptr;
	}

	CPartRainDrop* pParticle = new CPartRainDrop();

	// SBO: nullptr at the particle cap
	if( !pParticle )
	{
		return nullptr;
	}

	pParticle->m_splashAllowed = rainData.SplashesAllowed();
	pParticle->m_rippleAllowed = rainData.RipplesAllowed();
	pParticle->m_splashParams = rainData.splashParticleParams;
	pParticle->m_rippleParams = rainData.rippleParticleParams;
	pParticle->m_pRainSplash = rainData.splashSprite;
	pParticle->m_pRipple = rainData.rippleSprite;

	pParticle->InitializeSprite( vecOrigin, g_vecZero, rainData.rainSprite, rainData.raindropParticleParams.GetSize(), rainData.raindropParticleParams.brightness );

	strcpy( pParticle->m_szClassname, "particle_rain" );

	pParticle->m_flStretchY = rainData.raindropStretchY;

	if (rainData.RaindropsAffectedByWind())
	{
		// SBO 2.0: the weather's own wind (Spirit: every drop the same, plus the spread)
		if (rainData.HasFixedWind())
		{
			pParticle->m_vVelocity.x = rainData.windX;
			pParticle->m_vVelocity.y = rainData.windY;
		}
		else
		{
			pParticle->m_vVelocity.x = m_vecWind.x * Com_RandomFloat( 1.0f, 2.0f );
			pParticle->m_vVelocity.y = m_vecWind.y * Com_RandomFloat( 1.0f, 2.0f );
		}
		rainData.AddWindSpread( pParticle->m_vVelocity );
	}
	else
	{
		pParticle->m_vVelocity.x = pParticle->m_vVelocity.y = 0.0f;
	}

	pParticle->m_vVelocity.z = -rainData.GetRaindropFallingSpeed();

	int flags = TRI_COLLIDEWORLD | TRI_COLLIDEKILL | TRI_WATERTRACE;
	if (!rainData.CanGoThroughBrushEntities())
		flags |= TRI_COLLIDEBRUSHENTS;
	if (rainData.ShouldTraceWater())
		flags |= TRI_WATERTRACEKILL;
	pParticle->SetCollisionFlags(flags);

	pParticle->m_flGravity = 0;

	pParticle->SetCullFlag( CULL_PVS );
	pParticle->SetLightFlag( rainData.raindropParticleParams.lightFlag );

	pParticle->m_iRendermode = rainData.raindropParticleParams.renderMode;

	pParticle->m_vColor = rainData.raindropParticleParams.color;

	pParticle->m_flDieTime = gEngfuncs.GetClientTime() + rainData.raindropLife;

	return pParticle;
}

CPartWind* CEnvironment::CreateWindParticle( const Vector& vecOrigin, const RainData& rainData )
{
	if( !rainData.windPuffSprite )
	{
		return nullptr;
	}

	CPartWind* pParticle = new CPartWind();

	// SBO: nullptr at the particle cap
	if( !pParticle )
	{
		return nullptr;
	}

	Vector vecPartOrigin = vecOrigin;

	float particleSize = rainData.windParticleParams.GetSize();

	vecPartOrigin.z += particleSize * 0.3f;

	pParticle->InitializeSprite(
		vecPartOrigin, g_vecZero,
		rainData.windPuffSprite,
		particleSize,
		rainData.windParticleParams.brightness );

	pParticle->m_iNumFrames = rainData.windPuffSprite->numframes;

	strcpy( pParticle->m_szClassname, "wind_particle" );

	//pParticle->m_iFrame = UTIL_RandomLong( m_pGasPuffSprite->numframes / 2, m_pGasPuffSprite->numframes );

	// SBO 2.0: the puffs blow with the weather's own wind when it has one
	const Vector vecWind = rainData.HasFixedWind() ? Vector( rainData.windX, rainData.windY, 0.0f ) : m_vecWind;
	pParticle->m_vVelocity.x = vecWind.x / Com_RandomFloat( 1.0f, 2.0f );
	pParticle->m_vVelocity.y = vecWind.y / Com_RandomFloat( 1.0f, 2.0f );

	if( Com_RandomFloat( 0.0, 1.0 ) < 0.1 )
	{
		pParticle->m_vVelocity.x *= 0.5;
		pParticle->m_vVelocity.y *= 0.5;
	}

	pParticle->SetCollisionFlags( TRI_COLLIDEWORLD );
	pParticle->m_flGravity = 0;

	pParticle->m_iRendermode = rainData.windParticleParams.renderMode;

	pParticle->SetCullFlag( CULL_PVS );
	pParticle->SetLightFlag( rainData.windParticleParams.lightFlag );
	pParticle->SetRenderFlag( RENDER_FACEPLAYER );

	pParticle->m_vAVelocity.z = Com_RandomFloat( -1.0, 1.0 );

	pParticle->m_flScaleSpeed = 0.4;
	pParticle->m_flDampingTime = 0;

	pParticle->m_iFrame = 0;

	pParticle->m_flMass = 1.0f;
	pParticle->m_flBounceFactor = 0;
	pParticle->m_vColor = rainData.windParticleParams.color;

	pParticle->m_flFadeSpeed = -1.0f;

	pParticle->m_flDieTime = gEngfuncs.GetClientTime() + rainData.windpuffLife;

	return pParticle;
}

void CEnvironment::CreateSnowFlake( const Vector& vecOrigin, const SnowData& snowData )
{
	if( !snowData.snowSprite )
	{
		return;
	}

	CPartSnowFlake* pParticle = new CPartSnowFlake();

	// SBO: nullptr at the particle cap
	if( !pParticle )
	{
		return;
	}

	pParticle->m_targetBrightness = snowData.snowflakeParticleParams.brightness;

	pParticle->InitializeSprite(
		vecOrigin, g_vecZero,
		snowData.snowSprite,
		snowData.snowflakeParticleParams.GetSize(), snowData.snowflakeInitialBrightness );

	strcpy( pParticle->m_szClassname, "snow_particle" );

	pParticle->m_iNumFrames = snowData.snowSprite->numframes;

	if (snowData.SnowflakesAffectedByWind())
	{
		// SBO 2.0: the weather's own wind (Spirit: every flake the same, plus the spread)
		if (snowData.HasFixedWind())
		{
			pParticle->m_vVelocity.x = snowData.windX;
			pParticle->m_vVelocity.y = snowData.windY;
		}
		else
		{
			pParticle->m_vVelocity.x = m_vecWind.x / Com_RandomFloat( 1.0, 2.0 );
			pParticle->m_vVelocity.y = m_vecWind.y / Com_RandomFloat( 1.0, 2.0 );
		}
		snowData.AddWindSpread( pParticle->m_vVelocity );
	}
	else
	{
		pParticle->m_vVelocity.x = pParticle->m_vVelocity.y = 0.0f;
	}

	pParticle->m_vVelocity.z = -snowData.GetSnowflakeFallingSpeed();

	int flags = TRI_COLLIDEWORLD;
	if (!snowData.CanGoThroughBrushEntities())
		flags |= TRI_COLLIDEBRUSHENTS;
	if (snowData.ShouldTraceWater())
		flags |= TRI_WATERTRACE;
	pParticle->SetCollisionFlags(flags);

	const float flFrac = Com_RandomFloat( 0.0, 1.0 );

	if( flFrac >= 0.1 )
	{
		if( flFrac < 0.2 )
		{
			pParticle->m_vVelocity.z = -65.0;
		}
		else if( flFrac < 0.3 )
		{
			pParticle->m_vVelocity.z = -75.0;
		}
	}
	else if (!snowData.HasFixedWind())	// SBO 2.0: the CS wind's gusts; a fixed wind is the same for every flake
	{
		pParticle->m_vVelocity.x *= 0.5;
		pParticle->m_vVelocity.y *= 0.5;
	}

	pParticle->m_iRendermode = snowData.snowflakeParticleParams.renderMode;

	pParticle->SetCullFlag( CULL_PVS );
	pParticle->SetLightFlag( snowData.snowflakeParticleParams.lightFlag );
	pParticle->SetRenderFlag( RENDER_FACEPLAYER );

	pParticle->m_flScaleSpeed = 0;
	pParticle->m_flDampingTime = 0;
	pParticle->m_iFrame = 0;
	pParticle->m_flMass = 1.0;

	pParticle->m_flGravity = 0;
	pParticle->m_flBounceFactor = 0;

	pParticle->m_vColor = snowData.snowflakeParticleParams.color;

	pParticle->m_flDieTime = gEngfuncs.GetClientTime() + snowData.snowflakeLife;

	pParticle->m_bSpiral = Com_RandomLong( 0, 1 ) != 0;

	pParticle->m_flSpiralTime = gEngfuncs.GetClientTime() + Com_RandomLong( 2, 4 );
}

int CEnvironment::MsgFunc_ReceiveW(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	const int weatherType = READ_BYTE();

	// SBO: the server resends ReceiveW on every respawn; replace the weather instead of adding a duplicate. SBO 2.0:
	// every whole-map weather goes, the server's env_rain / env_snow too (SBO's server sends 0 and then those in
	// WeatherVol); the volumes stay
	RemoveGlobalWeather();

	if( weatherType == 0 )
	{
		m_iSavedWeatherType = 0;
		return 1;
	}

	if (weatherType < 1 || weatherType > 2)
		return 1;

	m_iSavedWeatherType = weatherType;

	RestoreWeather();

	return 1;
}

// SBO 2.0: a rain or snow (server: triggers.cpp CWeatherBase::Send): an env_rain / env_snow (whole-map weather around
// the view, like ReceiveW's) or, with the brush flag, a func_rainvolume / func_snowvolume. It keeps the server's entity
// index, so a resend (respawn, switched on / off, a key changed at run time) replaces it; type 0 removes it. An
// intensity, update period, size, speed or life of 0 keeps the CS default. The wind: with WEATHER_FIXED_WIND the
// weather's own (wind x / y), else the CS wind; the spread (0: none) on either. Volume spawning is cs16-client's
// (WeatherData::IsVolume: anywhere in the box, PVS check of the box).
int CEnvironment::MsgFunc_WeatherVol(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	const int entIndex = READ_SHORT();
	const int type = READ_BYTE();
	const int flags = READ_SHORT() & 0xFFFF;
	const int intensity = READ_SHORT();
	const float updatePeriod = READ_SHORT() / 100.0f;
	Vector absmin, absmax;
	absmin.x = READ_COORD();
	absmin.y = READ_COORD();
	absmin.z = READ_COORD();
	absmax.x = READ_COORD();
	absmax.y = READ_COORD();
	absmax.z = READ_COORD();
	const int size1 = READ_SHORT();
	const int size2 = READ_SHORT();
	const int speed1 = READ_SHORT();
	const int speed2 = READ_SHORT();
	const float life = READ_SHORT() / 100.0f;
	const float windX = (float)READ_SHORT();
	const float windY = (float)READ_SHORT();
	const int randX = READ_SHORT();
	const int randY = READ_SHORT();

	// index 0 is the global weather (ReceiveW); a volume never has it
	if ( entIndex <= 0 )
		return 1;

	const bool bVolume = ( flags & WEATHER_BRUSH_ENTITY ) != 0;

	RemoveRain( entIndex );
	RemoveSnow( entIndex );
	auto IsIndex = [entIndex]( const WeatherData& data ) { return data.entIndex == entIndex; };
	m_savedRains.erase( std::remove_if( m_savedRains.begin(), m_savedRains.end(), IsIndex ), m_savedRains.end() );
	m_savedSnows.erase( std::remove_if( m_savedSnows.begin(), m_savedSnows.end(), IsIndex ), m_savedSnows.end() );

	if ( type != 1 && type != 2 )
		return 1;
	if ( bVolume && !( absmax.x > absmin.x && absmax.y > absmin.y && absmax.z > absmin.z ) )
		return 1;

	auto SetBase = [&]( WeatherData& data )
	{
		data.entIndex = entIndex;
		data.flags = flags;
		data.windX = windX;
		data.windY = windY;
		data.windRandX = randX > 0 ? (float)randX : 0.0f;
		data.windRandY = randY > 0 ? (float)randY : 0.0f;
		if ( intensity > 0 )
			data.intensity = (short)( intensity > 1000 ? 1000 : intensity );
		if ( updatePeriod > 0 )
			data.updatePeriod = updatePeriod < 0.05f ? 0.05f : updatePeriod;
		if ( bVolume )
		{
			data.absmin = absmin;
			data.absmax = absmax;
		}
	};

	if ( type == 1 )
	{
		RainData rainData;
		SetBase( rainData );
		if ( size1 > 0 )
			rainData.raindropParticleParams.minSize = rainData.raindropParticleParams.maxSize = (float)size1;
		if ( size2 > 0 )
			rainData.raindropStretchY = size2 / 2.0f;	// the drop is stretched to twice this (80 units by default)
		if ( speed1 > 0 )
			rainData.raindropMinSpeed = (float)speed1;
		if ( speed2 > 0 )
			rainData.raindropMaxSpeed = (float)speed2;
		if ( life > 0 )
			rainData.raindropLife = life;

		SetRainSprites( rainData );
		AddRain( rainData );
		if ( !bVolume )
			m_savedRains.push_back( rainData );
	}
	else
	{
		SnowData snowData;
		SetBase( snowData );
		if ( size1 > 0 )
			snowData.snowflakeParticleParams.minSize = size1 / 10.0f;
		if ( size2 > 0 )
			snowData.snowflakeParticleParams.maxSize = size2 / 10.0f;
		if ( speed1 > 0 )
			snowData.snowflakeMinSpeed = (float)speed1;
		if ( speed2 > 0 )
			snowData.snowflakeMaxSpeed = (float)speed2;
		if ( life > 0 )
			snowData.snowflakeLife = life;

		SetSnowSprites( snowData );
		AddSnow( snowData );
		if ( !bVolume )
			m_savedSnows.push_back( snowData );
	}
	return 1;
}

void CEnvironment::RemoveRain(int entIndex)
{
	auto it = std::find_if(m_rains.begin(), m_rains.end(), [&](const RainData& data) {
		return data.entIndex == entIndex;
	});
	if (it != m_rains.end())
		m_rains.erase(it);
}

void CEnvironment::RemoveGlobalWeather()
{
	m_rains.erase(std::remove_if(m_rains.begin(), m_rains.end(), [](const RainData& data) {
		return !data.IsVolume();
	}), m_rains.end());
	m_snows.erase(std::remove_if(m_snows.begin(), m_snows.end(), [](const SnowData& data) {
		return !data.IsVolume();
	}), m_snows.end());
	m_savedRains.clear();
	m_savedSnows.clear();
}

void CEnvironment::AddRain(const RainData &rainData)
{
	auto it = std::find_if(m_rains.begin(), m_rains.end(), [&](const RainData& data) {
		return data.entIndex == rainData.entIndex;
	});
	if (it != m_rains.end())
	{
		gEngfuncs.Con_DPrintf("Rain with index %d already exists!\n", rainData.entIndex);
		return;
	}
	m_rains.push_back(rainData);
}

void CEnvironment::RemoveSnow(int entIndex)
{
	auto it = std::find_if(m_snows.begin(), m_snows.end(), [&](const SnowData& data) {
		return data.entIndex == entIndex;
	});
	if (it != m_snows.end())
		m_snows.erase(it);
}

void CEnvironment::AddSnow(const SnowData &snowData)
{
	auto it = std::find_if(m_snows.begin(), m_snows.end(), [&](const SnowData& data) {
		return data.entIndex == snowData.entIndex;
	});
	if (it != m_snows.end())
	{
		gEngfuncs.Con_DPrintf("Snow with index %d already exists!\n", snowData.entIndex);
		return;
	}
	m_snows.push_back(snowData);
}

model_t* CEnvironment::LoadSprite(const char* spriteName)
{
	// SBO: only hand existing, valid sprite files to the engine (a missing sprite disables that particle type)
	int iLength = 0;
	byte* pFile = gEngfuncs.COM_LoadFile( const_cast<char*>( spriteName ), 5, &iLength );
	const bool bValid = pFile && iLength > 4 && !memcmp( pFile, "IDSP", 4 );

	if( pFile )
		gEngfuncs.COM_FreeFile( pFile );

	if( !bValid )
	{
		gEngfuncs.Con_DPrintf( "Weather: missing or invalid sprite %s\n", spriteName );
		return nullptr;
	}

	const HSPRITE hSprite = SPR_Load(spriteName);
	if( !hSprite )
		return nullptr;

	model_t* pSprite = const_cast<model_t*>(gEngfuncs.GetSpritePointer(hSprite));
	if( !pSprite || pSprite->numframes <= 0 )
		return nullptr;

	return pSprite;
}
