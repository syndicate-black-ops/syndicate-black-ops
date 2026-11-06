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
// Particle manager interface, ported from cs16-client cl_dll/particleman (c1604b6). See docs/WEATHER.md.

#pragma once
#ifndef PARTICLEMAN_H
#define PARTICLEMAN_H

#include "CBaseParticle.h"

class IParticleMan
{
public:
	virtual ~IParticleMan() {}
	virtual void SetUp(cl_enginefunc_t* pEnginefuncs) = 0;
	virtual void Update() = 0;
	virtual void SetVariables(float flGravity, Vector vViewAngles) = 0;
	virtual void ResetParticles() = 0;
	virtual void ApplyForce(Vector vOrigin, Vector vDirection, float flRadius, float flStrength, float flDuration) = 0;

	//Use this if you want to create a new particle without any overloaded functions, Think, Touch, etc.
	//Just call this function, set the particle's behavior and let it rip.
	virtual CBaseParticle* CreateParticle(Vector org, Vector normal, model_s* sprite, float size, float brightness, const char* classname) = 0;

	virtual void SetRender(int iRender) = 0;
};

extern IParticleMan* g_pParticleMan;
#endif
