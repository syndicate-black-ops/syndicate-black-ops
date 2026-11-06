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
#ifndef FUNC_BREAK_H
#define FUNC_BREAK_H

typedef enum { expRandom, expDirected} Explosions;
typedef enum { matGlass = 0, matWood, matMetal, matFlesh, matCinderBlock, matCeilingTile, matComputer, matUnbreakableGlass, matRocks, matNone, matLastMaterial } Materials;

#define	NUM_SHARDS 6 // this many shards spawned when breakable objects break;

class CBreakable : public CBaseDelay
{
public:
	// basic functions
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData* pkvd);
	void EXPORT BreakTouch( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void DamageSound( void );

	// breakables use an overridden takedamage
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	// To spark when hit
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );

	BOOL IsBreakable( void );
	BOOL SparkWhenHit( void );

	int	 DamageDecal( int bitsDamageType );

	void EXPORT		Die( void );
	virtual int		ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	inline BOOL		Explodable( void ) { return ExplosionMagnitude() > 0; }
	inline int		ExplosionMagnitude( void ) { return pev->impulse; }
	inline void		ExplosionSetMagnitude( int magnitude ) { pev->impulse = magnitude; }

	static void MaterialSoundPrecache( Materials precacheMaterial );
	static void MaterialSoundRandom( edict_t *pEdict, Materials soundMaterial, float volume );
	static const char **MaterialSoundList( Materials precacheMaterial, int &soundCount );
	// SBO 2.0: shared with func_breakable_effect
	static const char *MaterialBustPrecache( Materials material );
	static char MaterialBustSound( edict_t *pEdict, Materials material, float fvol, int pitch );
	static void BreakModel( const Vector &vecSpot, const Vector &size, const Vector &vecVelocity, int shardModelIndex, int iGibs, char cFlag, float flGibScale = 0 );
	float m_flGibScale;	// SBO 2.0 (Featureful 0a0160c4 "gibscale"): the gibs' model scale; 0: TE_BREAKMODEL (scale 1)

	static const char *pSoundsWood[];
	static const char *pSoundsFlesh[];
	static const char *pSoundsGlass[];
	static const char *pSoundsMetal[];
	static const char *pSoundsConcrete[];
	static const char *pSpawnObjects[];

	static	TYPEDESCRIPTION m_SaveData[];

	Materials	m_Material;
	Explosions	m_Explosion;
	int			m_idShard;
	float		m_angle;
	int			m_iszGibModel;
	int			m_iszSpawnObject;

	// SBO 2.0, from Featureful (CBreakable, Half-Life 1 SDK license; func_pushable too; no map sets these): "m_iGibs"
	// number of gibs (0: the game decides, -1: none), "whenhit" fired at each hit with a Locus point at the hit
	// (flag 16 turns its direction around), "target_activator" (0 none, 1 this breakable, 2 the attacker or
	// activator), "spark_when_hit", "switch_texture_when_damaged" (frame 1 at half health or less), flags 4096 "Not
	// solid", 8192 smoke trails and 16384 transparent gibs
	int			m_iGibs;
	string_t	m_iszWhenHit;
	short		m_iTargetActivator;
	BOOL		m_fSparkWhenHit;
	BOOL		m_fSwitchTextureWhenDamaged;
	EHANDLE		m_hBreakActivator;	// who broke it (for target_activator 2)
	float		m_flInitialHealth;	// its health at spawn (switch_texture_when_damaged)
	BOOL		m_fBroken;			// Die ran (the "already broken" test for a Not Solid breakable)
	EHANDLE		m_hHitProxy;		// the Locus point of "whenhit" (an info_target, made at the first hit)
	void		UpdateOnRemove( void );
};

#endif	// FUNC_BREAK_H
