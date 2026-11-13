// SBO 2.0: the studio renderer's step interpolation (MOVETYPE_STEP, which the engine also sets for an EFLAG_SLERP
// model that moved between two animation times): the fraction of the move from the latched state to the current
// one, from the two animation times

#pragma once

#include <math.h>

// An animation time travels at 1/100 s (DT_TIMEWINDOW_BIG in delta.lst), decoded against each packet's own time, so
// an unchanged one can arrive a few ms off, either way. An interval under that is no new animation update and is
// treated as an equal one, as StudioEstimateInterpolant does: dividing by it drew a monster riding a train hundreds of
// units off it (dedicated servers)
#define STUDIO_ANIMTIME_RESOLUTION	0.01f

inline float StudioStepFraction( float clTime, float animtime, float prevanimtime )
{
	float interval = animtime - prevanimtime;
	if ( clTime < animtime + 1.0f && fabsf( interval ) >= STUDIO_ANIMTIME_RESOLUTION )
		return ( clTime - animtime ) / interval;
	return 0.0f;
}
