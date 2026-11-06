#pragma once
#include "util.h"
struct entvars_t { string_t classname, targetname; Vector origin, velocity, rendercolor; };
class CBaseEntity
{
public:
	virtual ~CBaseEntity() {}
	entvars_t *pev;
	int m_iIndex;
	int eoffset() { return m_iIndex; }
	virtual BOOL IsPlayer() { return FALSE; }
	virtual BOOL CalcPosition( CBaseEntity *pLocus, Vector *out ) { *out = pev->origin; return TRUE; }
	virtual BOOL CalcVelocity( CBaseEntity *pLocus, Vector *out ) { *out = pev->velocity; return TRUE; }
	virtual BOOL CalcRatio( CBaseEntity *pLocus, float *out ) { *out = 0; return TRUE; }
};
