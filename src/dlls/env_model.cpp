//=========================================================
// env_model -- an animated studio model with an "on" and an "off" sequence: loop, freeze or switch state when a
// sequence ends; optionally solid (a small box), dropped to the floor, animated by the clients, or sent to every
// client whatever it can see.
//
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license)
// dlls/effects.cpp CEnvModel, itself Spirit of Half-Life's (Laurie Cheers). Same class name, keys and flags.
// docs/ENV_MODEL.md.
//
// SBO changes (marked "SBO" below): "Always send" is a list in the game DLL read by AddToFullPack (client.cpp;
// Featureful: an entity flag field SBO doesn't have); "round_reset" (round_hooks.h); an unknown sequence is a console
// note (Featureful: at_error); "Solid (model hitboxes)" (flag 8, SBO 2.0: the engine collides with the model's
// hitboxes, docs/ENV_MODEL.md "Solid (model hitboxes)").
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"saverestore.h"
#include	"com_model.h"	// MAX_EDICTS
#include	"studio.h"
#include	"pm_materials.h"	// MAT_PROP_TAG, CHAR_TEX_*

#define SF_ENVMODEL_OFF				1
#define SF_ENVMODEL_DROPTOFLOOR		2
#define SF_ENVMODEL_SOLID			4
#define SF_ENVMODEL_SOLID_HITBOXES	8	// SBO 2.0: "Solid (model hitboxes)"
#define SF_ENVMODEL_CLIENTSIDEANIM	16
#define SF_ENVMODEL_IGNORE_PVS		32

// SBO: the entities AddToFullPack sends to every client, in its view or not ("Always send"); client.cpp
BOOL g_bAlwaysSend[ MAX_EDICTS ];

class CEnvModel : public CBaseAnimating
{
public:
	void Spawn( void );
	void Precache( void );
	void Think( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void RoundReset( void );
	void UpdateOnRemove( void );
	// SBO 2.1 (Spirit of Half-Life): On while it plays its "on" sequence, Off while it is off ("Initially Off")
	STATE GetState( CBaseEntity *pActivator ) { return ( pev->spawnflags & SF_ENVMODEL_OFF ) ? STATE_OFF : STATE_ON; }
	int	ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void SetSequence( void );
	void SetHitboxSolid( void );	// SBO 2.0
	void UpdateHitboxSolid( void );	// SBO 2.0

	string_t m_iszSequence_On;
	string_t m_iszSequence_Off;
	int m_iAction_On;
	int m_iAction_Off;
	float m_flFramerate_On;
	float m_flFramerate_Off;

	float m_savedFrame;
	float m_savedAnimTime;

	BOOL m_bRoundReset;		// SBO 2.0: "round_reset"
	int m_iSpawnFlags;		// SBO 2.0: the spawnflags of the map (Initially Off is the state)
	float m_flStartFrame;
	BOOL m_bHitboxSolid;	// SBO 2.0: collides with its hitboxes (SOLID_BBOX) while on, SOLID_NOT while off
	int m_iMaterial;		// SBO 2.0: "material" (func_breakable's list): bullet impacts on the hitboxes
	BOOL m_bMaterialSet;	// SBO 2.0: the map gave "material" (else Cinder Block)
};

TYPEDESCRIPTION CEnvModel::m_SaveData[] =
{
	DEFINE_FIELD( CEnvModel, m_iszSequence_On, FIELD_STRING ),
	DEFINE_FIELD( CEnvModel, m_iszSequence_Off, FIELD_STRING ),
	DEFINE_FIELD( CEnvModel, m_iAction_On, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvModel, m_iAction_Off, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvModel, m_flFramerate_On, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvModel, m_flFramerate_Off, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvModel, m_savedFrame, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvModel, m_savedAnimTime, FIELD_TIME ),
	DEFINE_FIELD( CEnvModel, m_bRoundReset, FIELD_BOOLEAN ),
	DEFINE_FIELD( CEnvModel, m_iSpawnFlags, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvModel, m_flStartFrame, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvModel, m_bHitboxSolid, FIELD_BOOLEAN ),
	DEFINE_FIELD( CEnvModel, m_iMaterial, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CEnvModel, CBaseAnimating );
LINK_ENTITY_TO_CLASS( env_model, CEnvModel );

// SBO 2.0: func_breakable's material list (func_break.h) -> the material registry's letter (docs/MATERIALS.md).
// Out of the list: Cinder Block (concrete, Half-Life's hit sound for anything that is not a texture)
#define ENVMODEL_DEFAULT_MATERIAL	4

static char EnvModel_MaterialLetter( int material )
{
	static const char letters[] = {
		CHAR_TEX_GLASS,		// 0 Glass
		CHAR_TEX_WOOD,		// 1 Wood
		CHAR_TEX_METAL,		// 2 Metal
		CHAR_TEX_FLESH,		// 3 Flesh
		CHAR_TEX_CONCRETE,	// 4 Cinder Block
		CHAR_TEX_TILE,		// 5 Ceiling Tile
		CHAR_TEX_COMPUTER,	// 6 Computer
		CHAR_TEX_GLASS,		// 7 Unbreakable Glass
		CHAR_TEX_GRAVEL,	// 8 Rocks
	};
	if ( material < 0 || material >= (int)( sizeof( letters ) / sizeof( letters[0] ) ) )
		material = ENVMODEL_DEFAULT_MATERIAL;
	return letters[ material ];
}

void CEnvModel::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszSequence_On" ) )
	{
		m_iszSequence_On = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszSequence_Off" ) )
	{
		m_iszSequence_Off = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iAction_On" ) )
	{
		m_iAction_On = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iAction_Off" ) )
	{
		m_iAction_Off = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_flFramerate_On" ) )
	{
		m_flFramerate_On = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_flFramerate_Off" ) )
	{
		m_flFramerate_Off = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "material" ) )	// SBO 2.0: "Solid (model hitboxes)" impacts
	{
		m_iMaterial = atoi( pkvd->szValue );
		m_bMaterialSet = TRUE;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "round_reset" ) )	// SBO 2.0 (round_hooks.h)
	{
		m_bRoundReset = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseAnimating::KeyValue( pkvd );
}

void CEnvModel::Spawn( void )
{
	// SBO 2.0: no model, the editor placeholder (models/editor/, the FGD default) or a file the server doesn't have
	// would end the map at the precache below or send the clients a file they can't get
	if ( UTIL_MapperModelUnusable( pev, STRING( pev->model ) ) )
	{
		REMOVE_ENTITY( ENT( pev ) );
		return;
	}

	// SBO: a studio model is required (SET_MODEL of nothing or of a sprite would leave no sequences)
	const char *szModel = STRING( pev->model );
	size_t n = szModel ? strlen( szModel ) : 0;
	if ( n < 5 || stricmp( szModel + n - 4, ".mdl" ) )
	{
		ALERT( at_console, "env_model at (%.0f %.0f %.0f) without a studio model: removed\n", pev->origin.x, pev->origin.y, pev->origin.z );
		pev->flags |= FL_KILLME;
		return;
	}

	m_iSpawnFlags = pev->spawnflags;
	m_flStartFrame = pev->frame;
	if ( !m_bMaterialSet )
		m_iMaterial = ENVMODEL_DEFAULT_MATERIAL;	// SBO 2.0: no "material" key: Cinder Block

	Precache();
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	UTIL_SetOrigin( pev, pev->origin );

	if ( pev->spawnflags & SF_ENVMODEL_SOLID_HITBOXES )
		SetHitboxSolid();	// SBO 2.0 (falls back to the 20-unit box below for a model without the hitbox flag)
	else if ( pev->spawnflags & SF_ENVMODEL_SOLID )
	{
		pev->solid = SOLID_SLIDEBOX;
		UTIL_SetSize( pev, Vector( -10, -10, -10 ), Vector( 10, 10, 10 ) );	// LRCT
	}

	if ( pev->spawnflags & SF_ENVMODEL_DROPTOFLOOR )
	{
		pev->origin.z += 1;
		DROP_TO_FLOOR( ENT( pev ) );
	}

	// SBO: Featureful's EFLAG_ALWAYS_SEND
	int index = entindex();
	if ( index > 0 && index < MAX_EDICTS )
		g_bAlwaysSend[ index ] = ( pev->spawnflags & SF_ENVMODEL_IGNORE_PVS ) != 0;

	SetBoneController( 0, 0 );
	SetBoneController( 1, 0 );

	// SBO 2.0: the map's Starting frame (Precache above set pev->frame to the saved frame, 0 on a new map; Featureful
	// read pev->frame after it, so the key never applied)
	SetSequence();
	if ( m_flStartFrame < 0 )
		pev->frame = RANDOM_LONG( 0, 255 );
	else
		pev->frame = m_flStartFrame;
	m_savedFrame = pev->frame;	// Client-side animation counts the sequence's end from here

	pev->nextthink = gpGlobals->time + 0.1;
}

void CEnvModel::Precache( void )
{
	PRECACHE_MODEL( (char *)STRING( pev->model ) );

	// SBO 2.0: the hit sounds of its material (as func_breakable precaches its material's sounds)
	if ( pev->spawnflags & SF_ENVMODEL_SOLID_HITBOXES )
		TEXTURETYPE_PrecacheMaterial( EnvModel_MaterialLetter( m_iMaterial ) );
	pev->frame = m_savedFrame;
	pev->animtime = m_savedAnimTime;
}

void CEnvModel::UpdateOnRemove( void )
{
	int index = entindex();
	if ( index > 0 && index < MAX_EDICTS )
		g_bAlwaysSend[ index ] = FALSE;	// SBO: the slot's next entity is not sent everywhere
	CBaseAnimating::UpdateOnRemove();
}

// SBO 2.0: "round_reset" 1: on or off as the map says, its sequence from the start
void CEnvModel::RoundReset( void )
{
	if ( !m_bRoundReset )
		return;

	pev->spawnflags = ( pev->spawnflags & ~SF_ENVMODEL_OFF ) | ( m_iSpawnFlags & SF_ENVMODEL_OFF );
	SetSequence();
	UpdateHitboxSolid();
	if ( m_flStartFrame < 0 )
		pev->frame = RANDOM_LONG( 0, 255 );
	else
		pev->frame = m_flStartFrame;
	m_savedFrame = pev->frame;
	pev->nextthink = gpGlobals->time + 0.1;
}

void CEnvModel::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( ShouldToggle( useType, !( pev->spawnflags & SF_ENVMODEL_OFF ) ) )
	{
		if ( pev->spawnflags & SF_ENVMODEL_OFF )
			pev->spawnflags &= ~SF_ENVMODEL_OFF;
		else
			pev->spawnflags |= SF_ENVMODEL_OFF;

		SetSequence();
		UpdateHitboxSolid();
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CEnvModel::Think( void )
{
	int iTemp;

	if ( !FBitSet( pev->spawnflags, SF_ENVMODEL_CLIENTSIDEANIM ) )
	{
		float flInterval = StudioFrameAdvance();
		pev->nextthink = gpGlobals->time + 0.1;
		DispatchAnimEvents( flInterval );
	}
	else
	{
		// Still do calculations, but save result to m_savedFrame
		float flInterval = ( gpGlobals->time - m_savedAnimTime );
		if ( flInterval <= 0.001 )
		{
			m_savedAnimTime = gpGlobals->time;
		}
		else
		{
			if ( !m_savedAnimTime )
				flInterval = 0.0;
			m_savedFrame += flInterval * m_flFrameRate * pev->framerate;
			m_savedAnimTime = gpGlobals->time;

			if ( m_savedFrame < 0.0 || m_savedFrame >= 256.0 )
			{
				if ( m_fSequenceLoops )
					m_savedFrame -= (int)( m_savedFrame / 256.0 ) * 256.0;
				else
					m_savedFrame = ( m_savedFrame < 0.0 ) ? 0 : 255;
				m_fSequenceFinished = TRUE;
			}
		}
	}

	if ( m_fSequenceFinished && !m_fSequenceLoops )
	{
		if ( pev->spawnflags & SF_ENVMODEL_OFF )
			iTemp = m_iAction_Off;
		else
			iTemp = m_iAction_On;

		switch ( iTemp )
		{
		case 2: // change state
			if ( pev->spawnflags & SF_ENVMODEL_OFF )
				pev->spawnflags &= ~SF_ENVMODEL_OFF;
			else
				pev->spawnflags |= SF_ENVMODEL_OFF;
			SetSequence();
			UpdateHitboxSolid();
			break;
		default: // remain frozen
			return;
		}
	}
	pev->nextthink = gpGlobals->time + 0.1;
}

void CEnvModel::SetSequence( void )
{
	string_t iszSeq;

	if ( pev->spawnflags & SF_ENVMODEL_OFF )
		iszSeq = m_iszSequence_Off;
	else
		iszSeq = m_iszSequence_On;

	if ( FStringNull( iszSeq ) )
		return;
	pev->sequence = LookupSequence( STRING( iszSeq ) );

	if ( pev->sequence == -1 )
	{
		ALERT( at_console, "env_model \"%s\": unknown sequence \"%s\"\n", STRING( pev->targetname ), STRING( iszSeq ) );
		pev->sequence = 0;
	}

	pev->frame = 0;
	ResetSequenceInfo();

	// SBO 2.0: a blank framerate is 1 in either state (the FGD's default; ResetSequenceInfo set 1). Featureful used
	// Framerate when on for the off sequence when Framerate when off was blank
	if ( pev->spawnflags & SF_ENVMODEL_OFF )
	{
		if ( m_flFramerate_Off )
			pev->framerate = m_flFramerate_Off;
	}
	else if ( m_flFramerate_On )
		pev->framerate = m_flFramerate_On;

	m_savedFrame = pev->frame;
	m_savedAnimTime = pev->animtime;

	if ( pev->spawnflags & SF_ENVMODEL_OFF )
		m_fSequenceLoops = m_iAction_Off == 1;
	else
		m_fSequenceLoops = m_iAction_On == 1;
}

//=========================================================
// SBO 2.0: "Solid (model hitboxes)" (flag 8; docs/ENV_MODEL.md "Solid (model hitboxes)").
//
// A model compiled with model flag 512 (`$flags 512`; Xash and ReHLDS name it STUDIO_TRACE_HITBOX, the HL SDK's
// const.h FL_ONGROUND by value) makes the engine collide with its hitboxes ($hbox) instead of the entity's box: player
// movement on the server (SOLID_BBOX only; a SOLID_SLIDEBOX physent has no studio model), the clients' movement
// prediction, bullets, monsters and projectiles. The entity box is only the engine's broad phase (SV_ClipToLinks), so
// it has to cover every hitbox in every pose: here, every frame of every sequence, turned by the entity's angles as
// the engine turns them (R_StudioHull: pitch inverted), with the bone controllers where the engine puts them (0x7F)
// and every blend. The engine ignores pev->scale for the hitboxes (and the clients predict at scale 1).
//=========================================================

#define STUDIO_TRACE_HITBOX		512		// studiohdr_t.flags: collide with the hitboxes
#define ENVMODEL_MAX_HITBOXES	128		// the engine's studio_hull[ 128 ] / studio_planes[ 768 ] (not checked by it)
#define ENVMODEL_MAX_FRAMES		256		// frames sampled per sequence and blend

static void EnvModel_AngleQuaternion( const float *angles, float *q )
{
	float sy = sin( angles[2] * 0.5f ), cy = cos( angles[2] * 0.5f );
	float sp = sin( angles[1] * 0.5f ), cp = cos( angles[1] * 0.5f );
	float sr = sin( angles[0] * 0.5f ), cr = cos( angles[0] * 0.5f );

	q[0] = sr * cp * cy - cr * sp * sy;
	q[1] = cr * sp * cy + sr * cp * sy;
	q[2] = cr * cp * sy - sr * sp * cy;
	q[3] = cr * cp * cy + sr * sp * sy;
}

static void EnvModel_QuaternionMatrix( const float *q, const float *pos, float m[3][4] )
{
	m[0][0] = 1.0f - 2.0f * q[1] * q[1] - 2.0f * q[2] * q[2];
	m[1][0] = 2.0f * q[0] * q[1] + 2.0f * q[3] * q[2];
	m[2][0] = 2.0f * q[0] * q[2] - 2.0f * q[3] * q[1];
	m[0][1] = 2.0f * q[0] * q[1] - 2.0f * q[3] * q[2];
	m[1][1] = 1.0f - 2.0f * q[0] * q[0] - 2.0f * q[2] * q[2];
	m[2][1] = 2.0f * q[1] * q[2] + 2.0f * q[3] * q[0];
	m[0][2] = 2.0f * q[0] * q[2] + 2.0f * q[3] * q[1];
	m[1][2] = 2.0f * q[1] * q[2] - 2.0f * q[3] * q[0];
	m[2][2] = 1.0f - 2.0f * q[0] * q[0] - 2.0f * q[1] * q[1];
	m[0][3] = pos[0];
	m[1][3] = pos[1];
	m[2][3] = pos[2];
}

static void EnvModel_ConcatTransforms( const float a[3][4], const float b[3][4], float out[3][4] )
{
	for ( int i = 0; i < 3; i++ )
	{
		for ( int j = 0; j < 4; j++ )
			out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + ( j == 3 ? a[i][3] : 0.0f );
	}
}

// the raw value of one compressed animation channel at an integer frame (the client's StudioCalcBonePosition walk,
// HL SDK cl_dll/StudioModelRenderer.cpp, without the blend to the next frame); end: the end of the model file
static BOOL EnvModel_AnimValue( const mstudioanim_t *panim, int channel, int frame, const byte *end, float *out )
{
	const mstudioanimvalue_t *pv = (const mstudioanimvalue_t *)( (const byte *)panim + panim->offset[channel] );
	int k = frame;
	for ( int guard = 0; guard < 4096; guard++ )
	{
		if ( (const byte *)( pv + 1 ) > end )
			return FALSE;
		if ( pv->num.total == 0 )
			return FALSE;
		if ( pv->num.total > k )
			break;
		k -= pv->num.total;
		pv += pv->num.valid + 1;
	}
	if ( pv->num.total <= k )
		return FALSE;
	int index = ( pv->num.valid > k ) ? k + 1 : pv->num.valid;
	if ( (const byte *)( pv + index + 1 ) > end )
		return FALSE;
	*out = pv[ index ].value;
	return TRUE;
}

// grows mins / maxs by the hitboxes of one pose (bone transforms of the model, before the entity's rotation), turned
// by rot
static void EnvModel_AddPose( const studiohdr_t *phdr, float (*bones)[3][4], const float rot[3][3], Vector &mins, Vector &maxs )
{
	const mstudiobbox_t *pbox = (const mstudiobbox_t *)( (const byte *)phdr + phdr->hitboxindex );
	for ( int i = 0; i < phdr->numhitboxes; i++ )
	{
		int b = pbox[i].bone;
		if ( b < 0 || b >= phdr->numbones )
			b = 0;
		for ( int c = 0; c < 8; c++ )
		{
			const float local[3] = { ( c & 1 ) ? pbox[i].bbmax[0] : pbox[i].bbmin[0],
				( c & 2 ) ? pbox[i].bbmax[1] : pbox[i].bbmin[1], ( c & 4 ) ? pbox[i].bbmax[2] : pbox[i].bbmin[2] };
			float model[3], world[3];
			for ( int r = 0; r < 3; r++ )
				model[r] = bones[b][r][0] * local[0] + bones[b][r][1] * local[1] + bones[b][r][2] * local[2] + bones[b][r][3];
			for ( int r = 0; r < 3; r++ )
			{
				world[r] = rot[r][0] * model[0] + rot[r][1] * model[1] + rot[r][2] * model[2];
				mins[r] = fminf( mins[r], world[r] );
				maxs[r] = fmaxf( maxs[r], world[r] );
			}
		}
	}
}

// the box (relative to the origin) covering every hitbox of the model in every frame of every sequence and blend,
// turned by angles as the engine turns them; FALSE if nothing could be measured
static BOOL EnvModel_HitboxBounds( const studiohdr_t *phdr, const Vector &angles, Vector &mins, Vector &maxs )
{
	if ( phdr->numbones <= 0 || phdr->numbones > MAXSTUDIOBONES )
		return FALSE;

	const byte *base = (const byte *)phdr;
	const byte *end = base + phdr->length;

	// the tables read below inside the file
	struct { int index, count, size; } tables[] = {
		{ phdr->boneindex, phdr->numbones, (int)sizeof( mstudiobone_t ) },
		{ phdr->hitboxindex, phdr->numhitboxes, (int)sizeof( mstudiobbox_t ) },
		{ phdr->seqindex, phdr->numseq, (int)sizeof( mstudioseqdesc_t ) },
		{ phdr->bonecontrollerindex, phdr->numbonecontrollers, (int)sizeof( mstudiobonecontroller_t ) } };
	for ( int t = 0; t < (int)( sizeof( tables ) / sizeof( tables[0] ) ); t++ )
	{
		if ( tables[t].count < 0 || tables[t].index < 0
			|| (long long)tables[t].index + (long long)tables[t].count * tables[t].size > phdr->length )
			return FALSE;
	}

	// the entity's rotation: R_StudioHull hands the bone setup (-pitch, yaw, roll); AngleMatrix (yaw * pitch) * roll
	float rot[3][3];
	{
		float a[3] = { -angles.x * (float)( M_PI / 180.0 ), angles.y * (float)( M_PI / 180.0 ), angles.z * (float)( M_PI / 180.0 ) };
		float sp = sin( a[0] ), cp = cos( a[0] ), sy = sin( a[1] ), cy = cos( a[1] ), sr = sin( a[2] ), cr = cos( a[2] );
		rot[0][0] = cp * cy;	rot[0][1] = sr * sp * cy - cr * sy;	rot[0][2] = cr * sp * cy + sr * sy;
		rot[1][0] = cp * sy;	rot[1][1] = sr * sp * sy + cr * cy;	rot[1][2] = cr * sp * sy - sr * cy;
		rot[2][0] = -sp;		rot[2][1] = sr * cp;				rot[2][2] = cr * cp;
	}

	// the bone controllers at the engine's value for hull traces (0x7F; the mouth closed)
	float adj[MAXSTUDIOCONTROLLERS] = { 0 };
	const mstudiobonecontroller_t *pctl = (const mstudiobonecontroller_t *)( base + phdr->bonecontrollerindex );
	for ( int j = 0; j < phdr->numbonecontrollers && j < MAXSTUDIOCONTROLLERS; j++ )
	{
		float value;
		if ( pctl[j].index <= 3 )
		{
			if ( pctl[j].type & STUDIO_RLOOP )
				value = 0x7F * ( 360.0f / 256.0f ) + pctl[j].start;
			else
				value = ( 1.0f - 0x7F / 255.0f ) * pctl[j].start + ( 0x7F / 255.0f ) * pctl[j].end;
		}
		else
			value = pctl[j].start;
		switch ( pctl[j].type & STUDIO_TYPES )
		{
		case STUDIO_XR: case STUDIO_YR: case STUDIO_ZR:
			adj[j] = value * (float)( M_PI / 180.0 );
			break;
		default:
			adj[j] = value;
			break;
		}
	}

	mins = Vector( 9999999, 9999999, 9999999 );
	maxs = Vector( -9999999, -9999999, -9999999 );
	BOOL bAny = FALSE;

	const mstudiobone_t *pbones = (const mstudiobone_t *)( base + phdr->boneindex );
	const mstudioseqdesc_t *pseq = (const mstudioseqdesc_t *)( base + phdr->seqindex );
	static float bones[MAXSTUDIOBONES][3][4];

	for ( int s = 0; s < phdr->numseq; s++ )
	{
		if ( pseq[s].seqgroup != 0 )
		{
			// animation in a demand-loaded file the game DLL can't read: the sequence's own box (the mesh, turned)
			float corners[3][2] = { { pseq[s].bbmin[0], pseq[s].bbmax[0] }, { pseq[s].bbmin[1], pseq[s].bbmax[1] },
				{ pseq[s].bbmin[2], pseq[s].bbmax[2] } };
			for ( int c = 0; c < 8; c++ )
			{
				float p[3] = { corners[0][c & 1], corners[1][( c >> 1 ) & 1], corners[2][( c >> 2 ) & 1] };
				for ( int r = 0; r < 3; r++ )
				{
					float w = rot[r][0] * p[0] + rot[r][1] * p[1] + rot[r][2] * p[2];
					mins[r] = fminf( mins[r], w );
					maxs[r] = fmaxf( maxs[r], w );
				}
			}
			bAny = TRUE;
			continue;
		}

		int numframes = pseq[s].numframes < 1 ? 1 : ( pseq[s].numframes > ENVMODEL_MAX_FRAMES ? ENVMODEL_MAX_FRAMES : pseq[s].numframes );
		int numblends = pseq[s].numblends < 1 ? 1 : pseq[s].numblends;
		for ( int blend = 0; blend < numblends; blend++ )
		{
			const mstudioanim_t *panim = (const mstudioanim_t *)( base + pseq[s].animindex ) + blend * phdr->numbones;
			if ( (const byte *)( panim + phdr->numbones ) > end || (const byte *)panim < base )
				return FALSE;

			for ( int f = 0; f < numframes; f++ )
			{
				for ( int b = 0; b < phdr->numbones; b++ )
				{
					float value[6];
					for ( int ch = 0; ch < 6; ch++ )
					{
						value[ch] = pbones[b].value[ch];
						float raw;
						if ( panim[b].offset[ch] && EnvModel_AnimValue( &panim[b], ch, f, end, &raw ) )
							value[ch] += raw * pbones[b].scale[ch];
						int ctl = pbones[b].bonecontroller[ch];
						if ( ctl >= 0 && ctl < MAXSTUDIOCONTROLLERS )
							value[ch] += adj[ctl];
					}
					float q[4], m[3][4];
					EnvModel_AngleQuaternion( value + 3, q );
					EnvModel_QuaternionMatrix( q, value, m );
					int parent = pbones[b].parent;
					if ( parent >= 0 && parent < b )
						EnvModel_ConcatTransforms( bones[parent], m, bones[b] );
					else
						memcpy( bones[b], m, sizeof( m ) );
				}
				EnvModel_AddPose( phdr, bones, rot, mins, maxs );
				bAny = TRUE;
			}
		}
	}

	if ( !bAny )
		return FALSE;

	// one unit for the turn between two frames (the engine blends them)
	mins = mins - Vector( 1, 1, 1 );
	maxs = maxs + Vector( 1, 1, 1 );
	return TRUE;
}

// Spawn: SOLID_BBOX and the box covering the turned hitboxes, or the fallback. Model without flag 512: the 20-unit
// box of "Solid (20 unit box)". 0 or more than 128 hitboxes: not solid (the engine would trace an empty set, "all
// solid" on the server and nothing in the clients' prediction, or write past its 128 hulls)
void CEnvModel::SetHitboxSolid( void )
{
	studiohdr_t *phdr = (studiohdr_t *)GET_MODEL_PTR( ENT( pev ) );
	const char *szModel = STRING( pev->model );

	if ( !phdr )
	{
		ALERT( at_warning, "env_model \"%s\" (%s): no studio model data: not solid\n", STRING( pev->targetname ), szModel );
		pev->solid = SOLID_NOT;
		return;
	}

	if ( phdr->numhitboxes <= 0 || phdr->numhitboxes > ENVMODEL_MAX_HITBOXES )
	{
		ALERT( at_warning, "env_model \"%s\": %s has %d hitboxes (1 to %d work): not solid\n",
			STRING( pev->targetname ), szModel, phdr->numhitboxes, ENVMODEL_MAX_HITBOXES );
		pev->solid = SOLID_NOT;
		return;
	}

	if ( !( phdr->flags & STUDIO_TRACE_HITBOX ) )
	{
		ALERT( at_warning, "env_model \"%s\": %s is not compiled for hitbox collision ($flags 512): the 20-unit box\n",
			STRING( pev->targetname ), szModel );
		pev->solid = SOLID_SLIDEBOX;
		UTIL_SetSize( pev, Vector( -10, -10, -10 ), Vector( 10, 10, 10 ) );
		return;
	}

	if ( pev->scale > 0.0f && pev->scale != 1.0f )
		ALERT( at_warning, "env_model \"%s\": %s collides at its unscaled size (scale %g is drawn only)\n",
			STRING( pev->targetname ), szModel, pev->scale );

	Vector mins, maxs;
	if ( !EnvModel_HitboxBounds( phdr, pev->angles, mins, maxs ) )
	{
		ALERT( at_warning, "env_model \"%s\": %s: its hitboxes could not be measured: not solid\n", STRING( pev->targetname ), szModel );
		pev->solid = SOLID_NOT;
		return;
	}

	m_bHitboxSolid = TRUE;
	pev->solid = ( pev->spawnflags & SF_ENVMODEL_OFF ) ? SOLID_NOT : SOLID_BBOX;
	UTIL_SetSize( pev, mins, maxs );

	// the material for the clients' bullet impacts (ev_hldm.cpp): entity state endpos, which env_model does not use
	pev->endpos = Vector( EnvModel_MaterialLetter( m_iMaterial ), MAT_PROP_TAG, 0 );
}

// on: SOLID_BBOX, off: SOLID_NOT (only with "Solid (model hitboxes)" working)
void CEnvModel::UpdateHitboxSolid( void )
{
	if ( !m_bHitboxSolid )
		return;

	int solid = ( pev->spawnflags & SF_ENVMODEL_OFF ) ? SOLID_NOT : SOLID_BBOX;
	if ( pev->solid == solid )
		return;
	pev->solid = solid;
	UTIL_SetOrigin( pev, pev->origin );	// relink: in or out of the solid lists
}
