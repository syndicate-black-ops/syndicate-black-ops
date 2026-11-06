/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
// Particle manager (particleman) memory, ported from cs16-client cl_dll/particleman (c1604b6), which is based on
// halflife-updated's re-creation of Valve's particleman.dll. Compiled into client.dll. See docs/WEATHER.md.

#pragma once
#ifndef MINIMEM_H
#define MINIMEM_H

// SBO: cl_util.h's fabs() macro breaks the C++ library headers
#pragma push_macro("fabs")
#undef fabs
#include <cstddef>
#include <vector>
#pragma pop_macro("fabs")

class CBaseParticle;

#define TRIANGLE_FPS 30

/**
*	@brief Simple allocator that uses a chunk-based pool to serve requests.
*/
class CMiniMem
{
public:
	// SBO: hard cap on live particles (cs16-client has none); Allocate returns nullptr beyond it. Sized for CS's
	// heaviest weather: snow at cl_weather 3 peaks around 9000 live flakes (rain at cl_weather 3 around 2500).
	static constexpr std::size_t MaxParticles = 16384;

private:
	static CMiniMem* _instance;

	std::vector<CBaseParticle*> _particles;
	std::size_t _visibleParticles = 0;

protected:
	// private constructor and destructor.
	CMiniMem() = default;
	~CMiniMem() = default;

public:
	void* Allocate(std::size_t sizeInBytes);

	void Deallocate(void* memory);

	void ProcessAll(); //Processes all

	void Reset(); //clears memory, setting all particles to not used.

	void Shutdown();

	int ApplyForce(Vector vOrigin, Vector vDirection, float flRadius, float flStrength);

	static CMiniMem* Instance();

	std::size_t GetTotalParticles() { return _particles.size(); }
	std::size_t GetDrawnParticles() { return _visibleParticles; }
};
#endif
