// SBO: particle emitters used by the HUD effects (CHudFlamme, CHudFumee). 1.38's rain/snow (CHudFallingPart,
// CFallingPart, CListeFall) was replaced by the CS 1.6 weather (environment.cpp, docs/WEATHER.md).
// Implemented in particules.cpp (between parsemsg.obj and pm_debug.obj).
// Layouts recovered from the HUD code (P4) and the emitter code (P5); unknown fields are placeholders.
// Names: the class names CParticulesEmetteur/CSmokeEmit come from RTTI. The other names are pinned by the order of
// SBO's incremental-link thunk table (a hash of each decorated name, see NOTES.md "ILT order"): Particule, CListeEmetteur,
// CListeFumee, CListeFall, SetEmetteur, Render, ChangeState, Ajouter, Afficher, Clear and CSmokeEmit/CFallingPart::Reset
// fit SBO's slots across several classes at once (recovered); the remaining method names are hash-tuned stand-ins.

#ifndef PARTICULES_H
#define PARTICULES_H

// size 0x58, allocated with new[] (emitters) or malloc (1.38's CFallingPart)
struct Particule
{
	Vector		origin;			// 0x00
	Vector		velocity;		// 0x0c (unused)
	float		r;				// 0x18
	float		g;				// 0x1c
	float		b;				// 0x20
	int			age;			// 0x24
	float		speed;			// 0x28
	int			wind;			// 0x2c
	bool		active;			// 0x30
	float		alpha;			// 0x34
	float		size;			// 0x38
	int			unknown3c;		// 0x3c
	float		maxdist;		// 0x40
	float		dist;			// 0x44
	float		startz;			// 0x48
	Vector		unknown4c;		// 0x4c
};

// size 0x58, vtable 0x1022b82c (flames)
class CParticulesEmetteur
{
public:
	virtual void InitializeParts( int iParticule );
	virtual bool SetEmetteur( int iNumParticules, int iHeight, Vector vecOrigin, float flSpeed, int iLife, int iSpread, float r, float g, float b, float a, int bActive );
	virtual void Render( double flTime );
	virtual void EvolueParticules( double flTime );

	~CParticulesEmetteur();
	void ChangeState( void );

	Particule	*m_pParticules;		// 0x04
	Vector		m_vecOrigin;		// 0x08
	Vector		m_vecVelocity;		// 0x14
	int			m_iHeight;			// 0x20
	int			m_iNumParticules;	// 0x24
	int			m_iLife;			// 0x28
	float		m_flSpeed;			// 0x2c
	HSPRITE		m_hSprite;			// 0x30
	int			m_iUnknown34;		// 0x34
	int			m_iSpread;			// 0x38
	float		m_flR;				// 0x3c
	float		m_flG;				// 0x40
	float		m_flB;				// 0x44
	float		m_flA;				// 0x48
	bool		m_bActive;			// 0x4c
	CParticulesEmetteur *m_pNext;	// 0x50
	int			m_iId;				// 0x54
};

// size 0x80, vtable 0x1022b860 (smoke)
class CSmokeEmit
{
public:
	virtual bool SetEmetteur( int iNumParticules, int iHeight, Vector vecOrigin, float flSpeed, int iLife, int iParam8, int iParam9, int iParam10, int iParam11, int iParam12, int iParam13, int iParam14, int iParam15, int iParam16, int iParam17, int bActive );
	virtual void Render( double flTime );
	virtual void UpdateMouvementPhysics( double flTime );
	virtual void Reset( int iParticule );

	~CSmokeEmit();
	bool ProduceSmokePuffNew( void );
	void ChangeState( void );

	// 0x04: padding after the vfptr (class is 8-aligned because of the double)
	Particule	*m_pParticules;		// 0x08
	Vector		m_vecOrigin;		// 0x0c
	Vector		m_vecVelocity;		// 0x18
	int			m_iHeight;			// 0x24
	int			m_iNumParticules;	// 0x28
	int			m_iLife;			// 0x2c
	float		m_flSpeed;			// 0x30
	HSPRITE		m_hSprite;			// 0x34
	int			m_iFrame;			// 0x38
	int			m_iParam8;			// 0x3c
	bool		m_bActive;			// 0x40
	int			m_iId;				// 0x44
	CSmokeEmit	*m_pNext;			// 0x48
	int			m_iR;				// 0x4c
	int			m_iG;				// 0x50
	int			m_iB;				// 0x54
	float		m_flLastEmit;		// 0x58
	int			m_iParam12;			// 0x5c
	int			m_iParam13;			// 0x60
	int			m_iParam14;			// 0x64
	int			m_iSpread;			// 0x68
	float		m_flWindX;			// 0x6c
	float		m_flWindY;			// 0x70
	double		m_flAlpha;			// 0x78
};

// Singly linked lists of emitters (first, last), size 8
class CListeEmetteur
{
public:
	CListeEmetteur();
	void Ajouter( CParticulesEmetteur *pEmetteur );
	void Afficher( float flTime );
	void Clear( void );
	void ChangeState( int iId );
	~CListeEmetteur();

	CParticulesEmetteur *m_pFirst;
	CParticulesEmetteur *m_pLast;
};

class CListeFumee
{
public:
	CListeFumee();
	void Ajouter( CSmokeEmit *pEmit );
	void Afficher( float flTime );
	void ChangeState( int iId );
	void Clear( void );
	~CListeFumee();

	CSmokeEmit *m_pFirst;
	CSmokeEmit *m_pLast;
};

#endif // PARTICULES_H
