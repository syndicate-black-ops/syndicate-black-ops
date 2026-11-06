// SBO 2.0 material system, client side (docs/MATERIALS.md): loads the registry (pm_materials.h) for each map, with
// the map's own materials file named by the server ("MatFile"), and draws the impact effects of bullet hits
// (debris chips, wall puffs, sparks, decals per material) on the particle manager (particleman/).
//
// The effects follow CS: Condition Zero Deleted Scenes' (EV_DebriSystem, CreateDebrisWallPuff, CreateCustomGib):
// behavior from static analysis of Valve's binaries, no code.

#include <cmath>
#include <cstring>
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "const.h"
#include "com_model.h"
#include "event_api.h"
#include "r_efx.h"
#include "dlight.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "pm_materials.h"
#include "particleman.h"

extern IParticleMan *g_pParticleMan;
extern const Vector g_vecZero;

static cvar_t *cl_effectsdetail;	// CS:CZ Deleted Scenes' client cvar: 0 no debris chips and wall puffs

static unsigned char *Materials_Load( const char *path, int *length )
{
	return gEngfuncs.COM_LoadFile( (char *)path, 5, length );
}

static void Materials_Free( unsigned char *data )
{
	gEngfuncs.COM_FreeFile( data );
}

static void Materials_Log( const char *text )
{
	gEngfuncs.Con_DPrintf( "%s", text );
}

// "MatFile": string, the map's materials file ("" none). The server sends it once per map to each client.
static int __MsgFunc_MatFile( const char *pszName, int iSize, void *pbuf )
{
	char path[ MAT_PATH_LEN ];

	BEGIN_READ( pbuf, iSize );
	strncpy( path, READ_STRING(), sizeof( path ) - 1 );
	path[ sizeof( path ) - 1 ] = 0;

	MAT_SetLog( Materials_Log );
	MAT_LoadGlobal( Materials_Load, Materials_Free );
	if ( path[ 0 ] && MAT_LoadMapFile( Materials_Load, Materials_Free, path, NULL, NULL ) < 0 )
		gEngfuncs.Con_DPrintf( "materials: the server's materials file %s is refused or missing\n", path );
	return 1;
}

// ---------------------------------------------------------------------------------------------------------------
// sprites: loaded once per map, only existing and valid files go to the engine
// ---------------------------------------------------------------------------------------------------------------

#define MATERIALS_SPRITES 32

static struct
{
	char		name[ MAT_SOUND_LEN ];
	model_t		*model;
} s_sprites[ MATERIALS_SPRITES ];
static int s_spriteCount;

model_t *Materials_Sprite( const char *name )
{
	int i, length = 0;
	byte *file;
	bool valid;

	if ( !name || !name[ 0 ] )
		return NULL;
	for ( i = 0; i < s_spriteCount; i++ )
	{
		if ( !strcmp( s_sprites[ i ].name, name ) )
			return s_sprites[ i ].model;
	}
	if ( s_spriteCount >= MATERIALS_SPRITES )
		return NULL;

	// a missing or broken sprite: no particles of that kind (remembered, not tried again)
	file = gEngfuncs.COM_LoadFile( (char *)name, 5, &length );
	valid = file && length > 4 && !memcmp( file, "IDSP", 4 );
	if ( file )
		gEngfuncs.COM_FreeFile( file );

	model_t *model = NULL;
	if ( valid )
	{
		HSPRITE sprite = SPR_Load( name );
		if ( sprite )
			model = const_cast<model_t *>( gEngfuncs.GetSpritePointer( sprite ) );
		if ( model && model->numframes <= 0 )
			model = NULL;
	}
	if ( !model )
		gEngfuncs.Con_DPrintf( "materials: missing or invalid sprite %s\n", name );

	strncpy( s_sprites[ s_spriteCount ].name, name, MAT_SOUND_LEN - 1 );
	s_sprites[ s_spriteCount ].name[ MAT_SOUND_LEN - 1 ] = 0;
	s_sprites[ s_spriteCount++ ].model = model;
	return model;
}

// ---------------------------------------------------------------------------------------------------------------
// particles
// ---------------------------------------------------------------------------------------------------------------

// a debris chip (CS:CZ DS CreateCustomGib): flies off the wall, slows down after damp_time, rests on floors and
// fades on walls
class CPartDebris : public CBaseParticle
{
public:
	void Think( float flTime ) override
	{
		if ( !m_bDamped && flTime >= m_flDampAt )
		{
			m_bDamped = true;
			m_vVelocity = m_vVelocity * m_flDampSpeed;
			m_flGravity = m_flAfterDampGravity;
		}

		// fade out over the last second
		if ( m_flDieTime - flTime < 1.0f )
			m_flBrightness = m_flStartBrightness * ( m_flDieTime - flTime > 0.0f ? m_flDieTime - flTime : 0.0f );

		CBaseParticle::Think( flTime );
	}

	void Touch( Vector pos, Vector normal, int index, bool enteringWater ) override
	{
		// on a wall or ceiling: gone soon; on the floor: it rests until its life ends
		if ( normal.z <= 0.5f )
		{
			const float flFade = gEngfuncs.GetClientTime() + 0.25f;
			if ( m_flDieTime > flFade )
				m_flDieTime = flFade;
		}
	}

	float m_flDampAt = 0.0f;
	float m_flDampSpeed = 1.0f;
	float m_flAfterDampGravity = 1.0f;
	float m_flStartBrightness = 255.0f;
	bool m_bDamped = false;
};

// a wall puff (CS:CZ DS CreateDebrisWallPuff): tinted smoke that drifts off the wall, rises, grows and fades. The
// sprite is SBO's own indexalpha puff (the weather's), which takes the material's color, instead of CS:CZ DS's
// large_smoke_01_ind.spr
static void Materials_Puff( const Vector &origin, const Vector &normal, const float *color, int index )
{
	model_t *sprite = Materials_Sprite( "sprites/gas_puff_01.spr" );
	if ( !sprite || !g_pParticleMan )
		return;

	CBaseParticle *puff = new CBaseParticle();
	if ( !puff )		// the particle cap (CMiniMem::MaxParticles)
		return;

	const float size = gEngfuncs.pfnRandomFloat( 15.0f, 20.0f ) + index * 2.0f;
	puff->InitializeSprite( origin + normal * gEngfuncs.pfnRandomFloat( 1.0f, 3.0f ), g_vecZero, sprite, size,
		gEngfuncs.pfnRandomFloat( 120.0f, 160.0f ) );
	puff->m_iRendermode = kRenderTransAlpha;
	puff->m_vColor = Vector( color[ 0 ], color[ 1 ], color[ 2 ] );
	puff->m_vVelocity = normal * gEngfuncs.pfnRandomFloat( 8.0f, 20.0f ) + Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( 2.0f, 6.0f ) );
	puff->m_flGravity = -0.1f;
	puff->m_flScaleSpeed = 0.6f;
	puff->m_flFadeSpeed = 2.0f;
	puff->m_vAVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( -2.0f, 2.0f ) );
	puff->m_flDieTime = gEngfuncs.GetClientTime() + 3.0f;
	puff->SetCullFlag( CULL_PVS );
	puff->SetLightFlag( LIGHT_INTENSITY );
	puff->SetRenderFlag( RENDER_FACEPLAYER_ROTATEZ );
}

static void Materials_Chip( const Vector &origin, const Vector &normal, const mat_fx_t *fx )
{
	if ( fx->debris_sprites <= 0 || !g_pParticleMan )
		return;

	model_t *sprite = Materials_Sprite( fx->debris_sprite[ gEngfuncs.pfnRandomLong( 0, fx->debris_sprites - 1 ) ] );
	if ( !sprite )
		return;

	CPartDebris *chip = new CPartDebris();
	if ( !chip )
		return;

	chip->InitializeSprite( origin, g_vecZero, sprite, fx->debris_size, fx->debris_brightness );
	chip->m_flStartBrightness = fx->debris_brightness;
	chip->m_iRendermode = fx->debris_additive ? kRenderTransAdd : kRenderTransAlpha;
	chip->m_vColor = Vector( fx->debris_color[ 0 ], fx->debris_color[ 1 ], fx->debris_color[ 2 ] );

	// along the normal with a spread; speed 50-200 times the material's factor
	Vector dir = normal + Vector( gEngfuncs.pfnRandomFloat( -0.5f, 0.5f ), gEngfuncs.pfnRandomFloat( -0.5f, 0.5f ),
		gEngfuncs.pfnRandomFloat( -0.2f, 0.6f ) );
	dir = dir.Normalize();
	chip->m_vVelocity = dir * ( gEngfuncs.pfnRandomFloat( 50.0f, 200.0f ) * fx->debris_speed );
	chip->m_vAVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( -5.0f, 5.0f ) );
	chip->m_flGravity = fx->debris_gravity;
	chip->m_flAfterDampGravity = fx->debris_afterdamp_gravity;
	chip->m_flDampSpeed = fx->debris_damp_speed;
	chip->m_flDampAt = gEngfuncs.GetClientTime() + fx->debris_damp_time;
	chip->m_flBounceFactor = 0.4f;
	chip->m_flDieTime = gEngfuncs.GetClientTime() + fx->debris_life;
	chip->SetCollisionFlags( TRI_COLLIDEWORLD );
	chip->SetCullFlag( CULL_PVS );
	chip->SetLightFlag( fx->debris_additive ? LIGHT_NONE : LIGHT_INTENSITY );
	chip->SetRenderFlag( RENDER_FACEPLAYER_ROTATEZ );
}

// sparks (CS:CZ DS: streaks, a flash and a light): the engine's spark effect (sprites/richo1.spr and streaks,
// Half-Life's own) and a short light
static void Materials_Sparks( const Vector &origin )
{
	Vector pos = origin;

	gEngfuncs.pEfxAPI->R_SparkEffect( pos, 8, -200, 200 );

	dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocDlight( 0 );
	if ( dl )
	{
		dl->origin = origin;
		dl->radius = 55;
		dl->color.r = 240;
		dl->color.g = 240;
		dl->color.b = 180;
		dl->die = gEngfuncs.GetClientTime() + 0.1f;
		dl->decay = 5;
	}
}

// the texture a trace hit, stripped, and its letter
char Materials_TraceTexture( pmtrace_t *pTrace, char *stripped )
{
	Vector start = Vector( pTrace->endpos ) + Vector( pTrace->plane.normal ) * 4.0f;
	Vector end = Vector( pTrace->endpos ) - Vector( pTrace->plane.normal ) * 4.0f;
	const char *name = gEngfuncs.pEventAPI->EV_TraceTexture( pTrace->ent, start, end );

	MAT_StripTextureName( name, stripped );
	return name ? MAT_FindTextureType( stripped ) : MAT_DefaultType();
}

// a bullet hit on a brush model whose material has effects (fx->defined); hlDecal: Half-Life's decal for it
void Materials_Impact( pmtrace_t *pTrace, const mat_fx_t *fx, const char *hlDecal )
{
	const Vector normal = pTrace->plane.normal;
	Vector origin = Vector( pTrace->endpos ) + normal * 5.0f;
	const bool detail = !cl_effectsdetail || cl_effectsdetail->value > 0.0f;
	int i;

	// Half-Life's bullet particles and ricochet (49 %), the material's ricochet sounds if it has some
	gEngfuncs.pEfxAPI->R_BulletImpactParticles( pTrace->endpos );
	const int iRand = gEngfuncs.pfnRandomLong( 0, 0x7FFF );
	if ( iRand < ( 0x7fff / 2 ) )
	{
		char hlSound[ 32 ];
		const char *sound;

		if ( fx->ricochet.count > 0 )
			sound = fx->ricochet.name[ iRand % fx->ricochet.count ];
		else
		{
			snprintf( hlSound, sizeof( hlSound ), "weapons/ric%d.wav", iRand % 5 + 1 );
			sound = hlSound;
		}
		gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, sound, 1.0, ATTN_NORM, 0, PITCH_NORM );
	}

	// decal: the material's ("none": no decal), else Half-Life's
	const char *decal = fx->decal[ 0 ] ? fx->decal : hlDecal;
	if ( decal && decal[ 0 ] && strcmp( decal, "none" ) && CVAR_GET_FLOAT( "r_decals" ) )
	{
		gEngfuncs.pEfxAPI->R_DecalShoot( gEngfuncs.pEfxAPI->Draw_DecalIndex( gEngfuncs.pEfxAPI->Draw_DecalIndexFromName( (char *)decal ) ),
			gEngfuncs.pEventAPI->EV_IndexFromTrace( pTrace ), 0, pTrace->endpos, 0 );
	}

	if ( fx->sparks )
		Materials_Sparks( pTrace->endpos );

	if ( !detail )
		return;

	// wall puffs, not under water
	if ( fx->smoke && gEngfuncs.PM_PointContents( origin, NULL ) != CONTENTS_WATER )
	{
		for ( i = 0; i < 4; i++ )
			Materials_Puff( origin, normal, fx->smoke_color, i );
	}

	if ( fx->debris_count > 0 )
	{
		const int count = fx->debris_count + gEngfuncs.pfnRandomLong( 0, 2 );
		for ( i = 0; i < count; i++ )
			Materials_Chip( origin, normal, fx );
	}
}

#ifdef SBO_TEST_COMMANDS
// test_cl_materials 1 (test builds only, docs/MATERIALS.md "Testing"): the client prints every footstep
// (pm_shared.c), bullet hit sound and impact (ev_hldm.cpp) with the texture and its letter
static cvar_t *test_cl_materials;
extern "C" int pm_test_materials;

bool Materials_TestPrint( void )
{
	return test_cl_materials && test_cl_materials->value != 0.0f;
}

void Materials_TestFrame( void )
{
	pm_test_materials = Materials_TestPrint() ? 1 : 0;
}
#endif

void Materials_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "MatFile", __MsgFunc_MatFile );
	cl_effectsdetail = CVAR_CREATE( "cl_effectsdetail", "1", FCVAR_ARCHIVE );
#ifdef SBO_TEST_COMMANDS
	test_cl_materials = CVAR_CREATE( "test_cl_materials", "0", 0 );
#endif
}

// a new map: the global files only, until the server names the map's file; sprites are loaded again
void Materials_VidInit( void )
{
	MAT_SetLog( Materials_Log );
	MAT_LoadGlobal( Materials_Load, Materials_Free );
	s_spriteCount = 0;
}
