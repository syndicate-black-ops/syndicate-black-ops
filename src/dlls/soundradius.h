//=========================================================
// SBO 2.0, from Featureful (dlls/soundradius.h, plats.cpp SoundAttenuation; Half-Life 1 SDK license): the
// "soundradius" key of movers, buttons and env_spark (FGD base class SoundRadius): -2 small, -1 medium, 0 default
// (large, ATTN_NORM: Half-Life), 1 huge, 2 enormous, 3 everywhere. One place for every entity that takes it.
//=========================================================
#ifndef SOUNDRADIUS_H
#define SOUNDRADIUS_H

enum
{
	SOUND_RADIUS_SMALL = -2,
	SOUND_RADIUS_MEDIUM = -1,
	SOUND_RADIUS_LARGE = 0,
	SOUND_RADIUS_HUGE = 1,
	SOUND_RADIUS_ENORMOUS = 2,
	SOUND_RADIUS_EVERYWHERE = 3,
};

inline float SoundRadiusAttenuation( int soundRadius )
{
	switch ( soundRadius )
	{
	case SOUND_RADIUS_SMALL:		return ATTN_IDLE;
	case SOUND_RADIUS_MEDIUM:		return ATTN_STATIC;
	case SOUND_RADIUS_HUGE:			return 0.5f;
	case SOUND_RADIUS_ENORMOUS:		return 0.25f;
	case SOUND_RADIUS_EVERYWHERE:	return ATTN_NONE;
	default:						return ATTN_NORM;
	}
}

#endif // SOUNDRADIUS_H
