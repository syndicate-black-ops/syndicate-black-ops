// Aurora particle system (Spirit of Half-Life, "02/08/02 November235: Particle System", LRC).
//
// SBO 2.0 port of LogicAndTrick's halflife-updated-spirit (branch spirit-1.8-bug-fixes, commit 2482306),
// cl_dll/particlesys.h, with the bug fixes, limits, emitter placement (target / attachment) and one-shot copies
// listed in docs/AURORA.md. HL SDK license, as the rest of the client.
#pragma once

class ParticleType;
class ParticleSystem;

// "collision" key (Spirit had it commented out, so the "bounce" that followed it was read as a key name)
#define COLLISION_NONE		0
#define COLLISION_DIE		1
#define COLLISION_BOUNCE	2

// Limits (docs/AURORA.md "Limits")
#define AURORA_DEFAULT_PARTICLES	100		// "particles" not given
#define AURORA_MAX_PER_SYSTEM		2048	// "particles" is clamped to 1 .. this
#define AURORA_MAX_ACTIVE			8192	// particles alive in all systems together; new ones wait for a free slot
#define AURORA_MAX_ALLOCATED		65536	// particles allocated by all systems together (about 11 MB)
#define AURORA_MAX_SPRAY_PER_FRAME	16		// children one particle sprays in one frame (rates above the frame rate)
#define AURORA_MAX_TYPES			64		// particle types in one script
#define AURORA_MAX_OVERLAYS			4		// overlay layers on one particle
#define AURORA_ONESHOT_GRACE		30		// seconds a stopped one-shot copy may keep particles alive at most
#define AURORA_LIGHT_INTERVAL		0.1f	// "lightmodel": seconds between two world light samples of one particle

#define MAX_TYPENAME 30

struct particle
{
	particle *nextpart;

	particle *m_pOverlay;		// for making multi-layered particles (not in the active list: owned by this particle)

	ParticleType *pType;

	Vector origin;
	Vector velocity;
	Vector accel;
	Vector m_vecWind;

	bool m_bBound;				// the emitter's main particle: follows the emitter, never dies on its own

	float m_fRed;
	float m_fGreen;
	float m_fBlue;
	float m_fRedStep;
	float m_fGreenStep;
	float m_fBlueStep;

	float m_fAlpha;
	float m_fAlphaStep;

	float frame;
	float m_fFrameStep;

	float m_fAngle;
	float m_fAngleStep;

	float m_fSize;
	float m_fSizeStep;

	float m_fDrag;

	float age;
	float age_death;
	float age_spray;

	Vector m_vecLight;			// "lightmodel": the world light at the particle, 0 .. 1 each
	float m_fLightAge;			// the age at which it is sampled again
};

class RandomRange
{
public:
	RandomRange()
	{
		m_fMin = m_fMax = 0;
		m_bDefined = false;
	}
	RandomRange( float fValue )
	{
		m_fMin = m_fMax = fValue;
		m_bDefined = true;
	}
	RandomRange( float fMin, float fMax )
	{
		m_fMin = fMin;
		m_fMax = fMax;
		m_bDefined = true;
	}
	RandomRange( const char *szToken );

	float m_fMax;
	float m_fMin;
	bool m_bDefined;

	float GetInstance() const
	{
		if ( m_fMin == m_fMax )
			return m_fMin;
		return gEngfuncs.pfnRandomFloat( m_fMin, m_fMax );
	}

	float GetOffset( float fBasis ) const
	{
		return GetInstance() - fBasis;
	}

	bool IsDefined() const
	{
		return m_bDefined;
	}
};

class ParticleType
{
public:
	ParticleType( ParticleType *pNext = NULL );

	bool m_bIsDefined;		// is this ParticleType just a placeholder?

	int m_iRenderMode;
	int m_iDrawCond;
	int m_iCollision;		// COLLISION_*
	RandomRange m_Bounce;
	RandomRange m_BounceFriction;

	RandomRange m_Life;

	RandomRange m_StartAlpha;
	RandomRange m_EndAlpha;
	RandomRange m_StartRed;
	RandomRange m_EndRed;
	RandomRange m_StartGreen;
	RandomRange m_EndGreen;
	RandomRange m_StartBlue;
	RandomRange m_EndBlue;

	RandomRange m_StartSize;
	RandomRange m_SizeDelta;
	RandomRange m_EndSize;

	RandomRange m_StartFrame;
	RandomRange m_EndFrame;
	RandomRange m_FrameRate;	// incompatible with EndFrame

	RandomRange m_StartAngle;
	RandomRange m_AngleDelta;

	RandomRange m_SprayRate;
	RandomRange m_SprayForce;
	RandomRange m_SprayPitch;
	RandomRange m_SprayYaw;
	ParticleType *m_pSprayType;

	RandomRange m_Gravity;
	RandomRange m_WindStrength;
	RandomRange m_WindYaw;

	HSPRITE m_hSprite;			// 0: nothing to draw (no "sprite", or not found)
	ParticleType *m_pOverlayType;

	RandomRange m_Drag;

	ParticleType *m_pNext;

	char m_szName[MAX_TYPENAME];

	// here is a particle system. Add a (set of) particles according to this type, and initialise their values.
	particle *CreateParticle( ParticleSystem *pSys );

	// initialise this particle. Does not define velocity or age.
	void InitParticle( particle *pPart, ParticleSystem *pSys, int iDepth = 0 );

	bool Collides() const { return m_iCollision != COLLISION_NONE; }
};

// Where the system's emitter is (the message's placement)
struct AuroraPlacement
{
	Vector origin;			// world position, or the offset from the target in the target's frame
	Vector angles;			// the spray's turn, added to the target's angles when following one
	int iTarget;			// entity index to follow, 0 = none
	int iAttachment;		// the target's attachment 0 .. 3, -1 = its origin
};

class ParticleSystem
{
public:
	ParticleSystem( int iId, const char *szFilename, const char *szFileText );
	~ParticleSystem( void );

	void AllocateParticles( int iParticles );
	void CalculateDistance( const Vector &vecView );

	ParticleType *GetType( const char *szName );
	ParticleType *AddPlaceholderType( const char *szName );
	void TypeLimitReached( const char *szName );
	ParticleType *ParseType( char *&szFile );
	void ParseKillCondition( const char *szValue );

	static float c_fCosTable[360 + 90];
	static bool c_bCosTableInit;

	// any angle in degrees (wraps; Spirit read outside the table for angles out of -360 .. 449)
	static float CosLookup( float angle );
	static float SinLookup( float angle );

	// General functions
	bool UpdateSystem( float frametime );	// false: the manager deletes the system
	void DrawSystem( const Vector &vecView, const Vector &forward, const Vector &right, const Vector &up );
	particle *ActivateParticle();		// adds one of the free particles to the active list; MAY RETURN NULL
	particle *AllocParticle();			// takes a free particle off the free list (overlays); MAY RETURN NULL
	void FreeParticle( particle *pPart );	// back to the free list, with its overlays

	// returns false if the particle has died
	bool UpdateParticle( particle *part, float frametime );
	void StepParticle( particle *part, float frametime );
	void DrawParticle( particle *part, const Vector &right, const Vector &up );

	// the emitter: position and turn this frame; false while it can't be placed (its target isn't in this frame)
	bool GetEmitter( Vector &origin, Vector &angles );

	void SetPlacement( const AuroraPlacement &placement ) { m_Placement = placement; }
	void SetOn( bool bOn ) { m_bOn = bOn; }
	bool IsOn() const { return m_bOn; }

	// a one-shot copy (Spawn Use): on for the main type's "lifetime" (5 s if it has none, 120 s at most)
	void StartOneShot( float flNow );

	// "killcondition": stops the system for good once its emitter is in those contents
	void CheckKillCondition( void );

	// "lightmodel": the particle's world light (sampled every AURORA_LIGHT_INTERVAL seconds of its age)
	const Vector &ParticleLight( particle *part );

	// Pointer to next system for linked list structure
	ParticleSystem *m_pNextSystem;

	particle *m_pActiveParticle;
	float m_fViewerDist;
	int m_iId;					// the env_particle's entity index; 0 for a one-shot copy (Spawn Use)
	bool m_bOneShot;
	float m_flStopTime;			// one-shot: the client time the emitter stops
	char m_szFile[64];
	bool m_bVisible;			// this frame: in the PVS and placed

private:
	// the block of allocated particles
	particle *m_pAllParticles;
	int m_iAllocated;
	particle *m_pFreeParticle;
	particle *m_pMainParticle;	// the "source" particle.

	ParticleType *m_pFirstType;
	int m_iTypes;
	bool m_bTypeLimitWarned;	// "too many particle types" printed

	ParticleType *m_pMainType;

	AuroraPlacement m_Placement;
	bool m_bOn;

	int m_iLightModel;			// "lightmodel": 0 unlit, 1 (and 2) tinted by the world light
	int m_iKillCondition;		// "killcondition": CONTENT_EMPTY / WATER / SOLID, 0 = none
	bool m_bKilled;				// the kill condition was met: emits no more (the particles alive finish)

	Vector m_vecEmitter;		// this frame's emitter position and turn (GetEmitter)
	Vector m_vecEmitterAngles;
	bool m_bHasEmitter;

	Vector m_vecMins;			// the particles' bounds after the last update (PVS test)
	Vector m_vecMaxs;
	bool m_bHasBounds;
};

// particlemgr.cpp: SPR_Load of a sprite that exists (0 and one console line otherwise)
HSPRITE Aurora_LoadSprite( const char *szName );

extern bool g_bAuroraQuiet;
// set while the particle editor builds its preview (aurora_editor.cpp): a script's problems go there, not to the console
typedef void ( *AuroraWarnSink )( const char *szLine );
extern AuroraWarnSink g_pfnAuroraWarnSink;
extern int g_iAuroraActiveParticles;
extern int g_iAuroraAllocatedParticles;
