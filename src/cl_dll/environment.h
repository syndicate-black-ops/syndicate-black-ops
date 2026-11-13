// environment.h -- Counter-Strike 1.6 weather (rain, snow) on the client.
//
// Ported from cs16-client cl_dll/environment.h (https://github.com/Velaron/cs16-client, c1604b6), the CS 1.6 path only
// (ReceiveW: 0 none, 1 rain, 2 snow). Left out: cs16-client's own Rain/Snow/WeatherPos messages (Featureful-style
// extensions, not CS). See docs/WEATHER.md.
//
// CS16Client: Copyright (C) 2015-2016 Flying With Gauss. This file is free software under the GNU General Public
// License v2 or later, with the special exception that permits linking with the Half-Life engine and Valve's MODs
// (see docs/WEATHER.md, "Licence").

#pragma once
#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

// SBO: include after hud.h (Vector, byte); SBO's cl_dll.h / util_vector.h have no include guards
#include "com_model.h"

// SBO: cl_util.h's fabs() macro breaks the C++ library headers
#pragma push_macro("fabs")
#undef fabs
#include <vector>
#pragma pop_macro("fabs")

class CPartRainDrop;
class CPartWind;

struct ParticleParams
{
	float minSize;
	float maxSize;
	Vector color;
	byte brightness;
	byte renderMode;
	short lightFlag;

	float GetSize() const;
};

struct WeatherData
{
	WeatherData();

	int entIndex;
	int flags;

	short intensity;
	float updatePeriod;
	short distance;

	short minHeight;
	short maxHeight;

	Vector location;

	Vector absmin;
	Vector absmax;

	float updateTime;

	// SBO 2.0: the server's wind_x / wind_y (units per second; used with WEATHER_FIXED_WIND) and wind_rand_x /
	// wind_rand_y (0: no spread)
	float windX;
	float windY;
	float windRandX;
	float windRandY;

	bool HasFixedWind() const;
	void AddWindSpread(Vector& velocity) const;

	bool IsLocalizedPoint() const;
	bool IsVolume() const;
	bool IsGlobal() const;
	bool AllowIndoors() const;
	bool DistanceIsRadius() const;
	bool NeedsPVSCheck() const;
	float RandomHeight() const;
	bool CanGoThroughBrushEntities() const;
	bool ShouldTraceWater() const;

	Vector GetWeatherOrigin(const Vector& globalWeatherOrigin) const;
	void GetBoundingBox(const Vector& weatherOrigin, Vector& mins, Vector& maxs) const;
	Vector GetRandomOrigin(const Vector& weatherOrigin) const;
};

struct RainData : public WeatherData
{
	RainData();

	ParticleParams raindropParticleParams;
	float raindropStretchY;
	float raindropMinSpeed;
	float raindropMaxSpeed;
	float raindropLife;

	float GetRaindropFallingSpeed() const;

	ParticleParams windParticleParams;
	float windpuffLife;

	ParticleParams splashParticleParams;
	ParticleParams rippleParticleParams;

	bool RaindropsAffectedByWind() const;
	bool WindPuffsAllowed() const;
	bool SplashesAllowed() const;
	bool RipplesAllowed() const;

	model_t* rainSprite;
	model_t* windPuffSprite;
	model_t* splashSprite;
	model_t* rippleSprite;
};

struct SnowData : public WeatherData
{
	SnowData();

	ParticleParams snowflakeParticleParams;
	byte snowflakeInitialBrightness;
	float snowflakeMinSpeed;
	float snowflakeMaxSpeed;
	float snowflakeLife;

	float GetSnowflakeFallingSpeed() const;

	bool SnowflakesAffectedByWind() const;

	model_t* snowSprite;
};

class CEnvironment
{
public:
	CEnvironment() = default;

	float GetOldTime() const { return m_flOldTime; }

	void Initialize();
	void Reset();
	void Clear();
	void Update();
	void RestoreWeather();

	int MsgFunc_ReceiveW(const char *pszName, int iSize, void *pbuf);
	// SBO 2.0: env_rain / env_snow with their keys and func_rainvolume / func_snowvolume (Featureful's volume weather
	// on the CS weather; docs/WEATHER.md)
	int MsgFunc_WeatherVol(const char *pszName, int iSize, void *pbuf);
	// SBO 2.0: CS:CZ Deleted Scenes' grass on the map's grass textures (letter A; docs/MATERIALS.md)
	int MsgFunc_GrassInfo(const char *pszName, int iSize, void *pbuf);

private:
	void RemoveRain(int entIndex);
	void AddRain(const RainData& rainData);
	void RemoveSnow(int entIndex);
	void AddSnow(const SnowData& snowData);
	// SBO 2.0: the whole-map weathers (not volumes): ReceiveW's (index 0) and the server's env_rain / env_snow
	void RemoveGlobalWeather();
	void SetRainSprites(RainData& rainData);
	void SetSnowSprites(SnowData& snowData);

	bool ShouldUpdateWind() const;
	void UpdateWind();
	void UpdateRain(const RainData& rainData);
	void UpdateSnow(const SnowData& snowData);

	CPartRainDrop* CreateRaindrop(const Vector& vecOrigin, const RainData& rainData);
	CPartWind* CreateWindParticle(const Vector& vecOrigin, const RainData& rainData);
	void CreateSnowFlake(const Vector& vecOrigin, const SnowData& snowData);

	model_t* LoadSprite(const char* spriteName);

	void ResetGrass();
	void UpdateGrass(const Vector& vecOrigin);
	void PlaceGrass(const Vector& vecOrigin, float innerRadius, float outerRadius, int quantity);
	bool MapHasGrassTextures() const;

	int m_iSavedWeatherType = 0;
private:
	Vector m_vecWeatherOrigin;

	model_t* m_pRainSprite = nullptr;
	model_t* m_pGasPuffSprite = nullptr;
	model_t* m_pRainSplash = nullptr;
	model_t* m_pRipple = nullptr;
	model_t* m_pSnowSprite = nullptr;

	Vector m_vecWind;

	Vector m_vecDesiredWindDirection;

	float m_flDesiredWindSpeed = 0.0f;
	float m_flNextWindChangeTime = 0.0f;

	float m_flOldTime = 0.0f;

	float m_flIdealYaw = 0.0f;
	float m_flWeatherValue = 0.0f;

	std::vector<RainData> m_rains;
	std::vector<SnowData> m_snows;

	// SBO 2.0: the whole-map weathers the server sent in WeatherVol (env_rain / env_snow with their keys), kept like
	// m_iSavedWeatherType for RestoreWeather (a video restart on the same map)
	std::vector<RainData> m_savedRains;
	std::vector<SnowData> m_savedSnows;

	// grass (GrassInfo)
	char m_szGrassSprite[64] = "";
	float m_flGrassFadeDist = 0.0f;
	float m_flGrassMinHeight = 0.0f;
	float m_flGrassMaxHeight = 0.0f;
	float m_flGrassDensity = 0.0f;		// pieces per pass (CS:CZ DS: the ring passes after the first)
	bool m_bGrassActive = false;
	bool m_bGrassPlaceInitial = false;
	float m_flGrassNextUpdate = 0.0f;
	Vector m_vecGrassOrigin;
	model_t* m_pGrassSprite1 = nullptr;
	model_t* m_pGrassSprite2 = nullptr;

private:
	CEnvironment( const CEnvironment& ) = delete;
	CEnvironment& operator=( const CEnvironment& ) = delete;
};

extern CEnvironment g_Environment;

#endif
