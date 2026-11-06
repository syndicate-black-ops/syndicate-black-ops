// Offline test of the server's model scale bone setup (src/dlls/animation.cpp Server_GetBlendingInterface,
// SV_StudioSetupBonesScaled; docs/MODEL_SCALE.md "Collision box and hitboxes"). run_test.sh builds it with
// animation.cpp.
//
// A stand-in for the engine hands its "own" bone setup to Server_GetBlendingInterface, as the engine does: a plain
// bone setup of a real model (frame decoding as the client's StudioCalcBoneQuaterion / StudioCalcBonePosition, no
// interpolation, no controllers, no blends). Then:
// 1. scale 0 and 1 (every entity without the key, every player): the bones are exactly the engine's (bit for bit);
// 2. scale s, the hitboxes (iBone -1): the six planes the engine makes of each hitbox from the bones (R_StudioHull:
//    normal = bone axis k, distance = normal . bone position + bbmin[k] / bbmax[k]) lie on the faces of the hitbox
//    as the client draws it (the model's root transform times s, StudioSetUpTransform), and a point just inside a
//    face is inside, one just outside is outside;
// 3. scale s, an attachment and a bone position (one bone and its parents): where the client's scaled transform
//    puts them;
// 4. the scale as the clients get it (1/256 steps, 0 = 1).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "../common/nowin.h"

typedef int BOOL;
#define TRUE	 1
#define FALSE	0
typedef int qboolean;
typedef unsigned char byte;
#include "utils_mathlib.h"
#include "const.h"
#include "progdefs.h"
#include "edict.h"
#include "eiface.h"
#include "studio.h"
#include "r_studioint.h"

enginefuncs_t g_engfuncs;
globalvars_t *gpGlobals;

extern "C" int Server_GetBlendingInterface( int version, struct sv_blending_interface_s **ppinterface,
	struct engine_studio_api_s *pstudio, float (*rotationmatrix)[3][4], float (*bonetransform)[MAXSTUDIOBONES][3][4] );
float UTIL_ModelScaleValue( float flScale );

// util.cpp's UTIL_ModelScale also checks the entity's class (monsters, items, ammo_reserve) and its .mdl; the
// stand-in edicts here have no game entity: every one counts as such a class
float UTIL_ModelScale( const entvars_t *pev ) { return UTIL_ModelScaleValue( pev->scale ); }
// head_scale.cpp's HeadScale_ForEdict gives a player's head scale and 1 for everything else; the stand-in edicts are
// not players
float HeadScale_ForEdict( const edict_t *pEdict ) { return 1.0f; }

static int g_failures, g_checks;
#define CHECK( cond, ... ) do { g_checks++; if ( !( cond ) ) { g_failures++; printf( "FAIL: " __VA_ARGS__ ); printf( "\n" ); } } while ( 0 )

static void AlertStub( ALERT_TYPE, char *szFmt, ... )
{
	va_list ap;
	va_start( ap, szFmt );
	printf( "  [alert] " );
	vprintf( szFmt, ap );
	va_end( ap );
}

// ---- the stand-in engine ----
static studiohdr_t *g_pHdr;
static float g_rotationmatrix[3][4];
static float g_bonetransform[MAXSTUDIOBONES][3][4];
static float g_flRootScale = 1.0f;	// the client's StudioSetUpTransform scale (the reference); 1 for the engine

static void *Stub_Mod_Extradata( struct model_s * ) { return g_pHdr; }

static void AngleMatrixT( const float *angles, float (*matrix)[4] )
{
	float angle, sr, sp, sy, cr, cp, cy;
	angle = angles[1] * ( M_PI * 2 / 360 ); sy = sin( angle ); cy = cos( angle );
	angle = angles[0] * ( M_PI * 2 / 360 ); sp = sin( angle ); cp = cos( angle );
	angle = angles[2] * ( M_PI * 2 / 360 ); sr = sin( angle ); cr = cos( angle );
	matrix[0][0] = cp * cy; matrix[1][0] = cp * sy; matrix[2][0] = -sp;
	matrix[0][1] = sr * sp * cy + cr * -sy; matrix[1][1] = sr * sp * sy + cr * cy; matrix[2][1] = sr * cp;
	matrix[0][2] = cr * sp * cy + -sr * -sy; matrix[1][2] = cr * sp * sy + -sr * cy; matrix[2][2] = cr * cp;
	matrix[0][3] = matrix[1][3] = matrix[2][3] = 0;
}

static void AngleQuaternionT( const float *angles, float *q )
{
	float angle, sr, sp, sy, cr, cp, cy;
	angle = angles[2] * 0.5; sy = sin( angle ); cy = cos( angle );
	angle = angles[1] * 0.5; sp = sin( angle ); cp = cos( angle );
	angle = angles[0] * 0.5; sr = sin( angle ); cr = cos( angle );
	q[0] = sr * cp * cy - cr * sp * sy;
	q[1] = cr * sp * cy + sr * cp * sy;
	q[2] = cr * cp * sy - sr * sp * cy;
	q[3] = cr * cp * cy + sr * sp * sy;
}

static void QuaternionMatrixT( const float *q, float (*m)[4] )
{
	m[0][0] = 1.0 - 2.0 * q[1] * q[1] - 2.0 * q[2] * q[2];
	m[1][0] = 2.0 * q[0] * q[1] + 2.0 * q[3] * q[2];
	m[2][0] = 2.0 * q[0] * q[2] - 2.0 * q[3] * q[1];
	m[0][1] = 2.0 * q[0] * q[1] - 2.0 * q[3] * q[2];
	m[1][1] = 1.0 - 2.0 * q[0] * q[0] - 2.0 * q[2] * q[2];
	m[2][1] = 2.0 * q[1] * q[2] + 2.0 * q[3] * q[0];
	m[0][2] = 2.0 * q[0] * q[2] + 2.0 * q[3] * q[1];
	m[1][2] = 2.0 * q[1] * q[2] - 2.0 * q[3] * q[0];
	m[2][2] = 1.0 - 2.0 * q[0] * q[0] - 2.0 * q[1] * q[1];
}

static void ConcatT( const float (*a)[4], const float (*b)[4], float (*o)[4] )
{
	for ( int i = 0; i < 3; i++ )
	{
		for ( int j = 0; j < 4; j++ )
			o[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + ( j == 3 ? a[i][3] : 0.0f );
	}
}

// one value of an animation channel at a whole frame
static float AnimValue( mstudioanim_t *panim, int channel, int frame )
{
	if ( panim->offset[channel] == 0 )
		return 0.0f;
	mstudioanimvalue_t *v = (mstudioanimvalue_t *)( (byte *)panim + panim->offset[channel] );
	int k = frame;
	while ( v->num.total <= k )
	{
		k -= v->num.total;
		v += v->num.valid + 1;
	}
	return ( v->num.valid > k ) ? v[k + 1].value : v[v->num.valid].value;
}

static void Stub_SetupBones( struct model_s *, float frame, int sequence, const vec3_t angles, const vec3_t origin,
	const byte *, const byte *, int iBone, const edict_t * )
{
	mstudioseqdesc_t *pseq = (mstudioseqdesc_t *)( (byte *)g_pHdr + g_pHdr->seqindex ) + sequence;
	// mstudioseqgroup_t as in the file (label[32], name[64], a 32-bit cache pointer, data): read by offset, the
	// struct has a pointer and so another layout in a 64-bit build. Sequence group 0 only (the model file itself)
	const byte *pgroup = (const byte *)g_pHdr + g_pHdr->seqgroupindex + pseq->seqgroup * 104;
	int groupData;
	memcpy( &groupData, pgroup + 100, sizeof( groupData ) );
	mstudioanim_t *panim = (mstudioanim_t *)( (byte *)g_pHdr + groupData + pseq->animindex );
	mstudiobone_t *pbones = (mstudiobone_t *)( (byte *)g_pHdr + g_pHdr->boneindex );
	int f = pseq->numframes > 1 ? (int)( ( pseq->numframes - 1 ) * frame / 256.0f ) : 0;

	AngleMatrixT( angles, g_rotationmatrix );
	for ( int i = 0; i < 3; i++ )
	{
		for ( int j = 0; j < 3; j++ )
			g_rotationmatrix[i][j] *= g_flRootScale;
		g_rotationmatrix[i][3] = origin[i];
	}

	(void)iBone;	// every bone (the engine sets up only iBone's chain: the test looks at that chain only)
	for ( int b = 0; b < g_pHdr->numbones; b++ )
	{
		float ang[3], pos[3], q[4], m[3][4];
		for ( int j = 0; j < 3; j++ )
		{
			pos[j] = pbones[b].value[j] + AnimValue( &panim[b], j, f ) * pbones[b].scale[j];
			ang[j] = pbones[b].value[j + 3] + AnimValue( &panim[b], j + 3, f ) * pbones[b].scale[j + 3];
		}
		AngleQuaternionT( ang, q );
		QuaternionMatrixT( q, m );
		m[0][3] = pos[0]; m[1][3] = pos[1]; m[2][3] = pos[2];
		int parent = pbones[b].parent;
		ConcatT( parent == -1 ? g_rotationmatrix : g_bonetransform[parent], m, g_bonetransform[b] );
	}
}

static sv_blending_interface_t g_EngineBlending = { SV_BLENDING_INTERFACE_VERSION, Stub_SetupBones };
static server_studio_api_t g_ServerStudio = { NULL, NULL, NULL, Stub_Mod_Extradata };

static float Dot3( const float *a, const float *b ) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

static void Transform( const float *in, const float (*m)[4], float *out )
{
	for ( int i = 0; i < 3; i++ )
		out[i] = Dot3( in, m[i] ) + m[i][3];
}

static float g_ref[MAXSTUDIOBONES][3][4];

int main( int argc, char **argv )
{
	if ( argc < 2 )
	{
		fprintf( stderr, "usage: %s model.mdl...\n", argv[0] );
		return 2;
	}

	g_engfuncs.pfnAlertMessage = AlertStub;

	// 4. the scale as the clients get it
	CHECK( UTIL_ModelScaleValue( 0.0f ) == 1.0f, "scale 0 -> 1" );
	CHECK( UTIL_ModelScaleValue( -2.0f ) == 1.0f, "scale -2 -> 1" );
	CHECK( UTIL_ModelScaleValue( 2.0f ) == 2.0f, "scale 2 -> 2" );
	CHECK( UTIL_ModelScaleValue( 0.5f ) == 0.5f, "scale 0.5 -> 0.5" );
	CHECK( UTIL_ModelScaleValue( 0.3f ) == 76.0f / 256.0f, "scale 0.3 -> 76/256, got %f", UTIL_ModelScaleValue( 0.3f ) );
	CHECK( UTIL_ModelScaleValue( 0.001f ) == 1.0f, "scale 0.001 -> 0 on the wire -> 1" );

	// the engine's call
	sv_blending_interface_t *pInterface = &g_EngineBlending;
	int ok = Server_GetBlendingInterface( SV_BLENDING_INTERFACE_VERSION, &pInterface,
		(struct engine_studio_api_s *)&g_ServerStudio, &g_rotationmatrix, &g_bonetransform );
	CHECK( ok == 1 && pInterface != &g_EngineBlending && pInterface->SV_StudioSetupBones, "interface taken" );
	sv_blending_interface_t *pAgain = pInterface;
	CHECK( Server_GetBlendingInterface( SV_BLENDING_INTERFACE_VERSION, &pAgain, (struct engine_studio_api_s *)&g_ServerStudio,
		&g_rotationmatrix, &g_bonetransform ) == 1 && pAgain == pInterface, "asked again with ours in place" );
	sv_blending_interface_t *pWrong = &g_EngineBlending;
	CHECK( Server_GetBlendingInterface( 2, &pWrong, (struct engine_studio_api_s *)&g_ServerStudio, &g_rotationmatrix,
		&g_bonetransform ) == 0 && pWrong == &g_EngineBlending, "another version: refused" );

	static edict_t ent;
	const float angles[3] = { 10.0f, 37.0f, -5.0f };		// pitch, yaw, roll
	const float origin[3] = { 128.0f, -64.0f, 32.0f };
	const byte controller[4] = { 127, 127, 127, 127 }, blending[2] = { 0, 0 };
	const float scales[] = { 0.25f, 0.5f, 1.5f, 2.0f, 3.0f, 4.0f };

	for ( int a = 1; a < argc; a++ )
	{
		FILE *fp = fopen( argv[a], "rb" );
		if ( !fp ) { printf( "FAIL: cannot open %s\n", argv[a] ); g_failures++; continue; }
		fseek( fp, 0, SEEK_END ); long len = ftell( fp ); fseek( fp, 0, SEEK_SET );
		g_pHdr = (studiohdr_t *)malloc( len );
		if ( fread( g_pHdr, 1, len, fp ) != (size_t)len ) { printf( "FAIL: read %s\n", argv[a] ); g_failures++; fclose( fp ); continue; }
		fclose( fp );

		mstudiobbox_t *pbox = (mstudiobbox_t *)( (byte *)g_pHdr + g_pHdr->hitboxindex );
		mstudioattachment_t *patt = (mstudioattachment_t *)( (byte *)g_pHdr + g_pHdr->attachmentindex );
		mstudiobone_t *pbones = (mstudiobone_t *)( (byte *)g_pHdr + g_pHdr->boneindex );
		printf( "%s: %d bones, %d hitboxes, %d attachments\n", argv[a], g_pHdr->numbones, g_pHdr->numhitboxes, g_pHdr->numattachments );

		int numseq = g_pHdr->numseq < 4 ? g_pHdr->numseq : 4;
		for ( int seq = 0; seq < numseq; seq++ )
		{
			mstudioseqdesc_t *pseq = (mstudioseqdesc_t *)( (byte *)g_pHdr + g_pHdr->seqindex ) + seq;
			if ( pseq->seqgroup != 0 )
				continue;
			for ( float frame = 0.0f; frame < 256.0f; frame += 100.0f )
			{
				// 1. unscaled: the engine's bones, bit for bit
				const float unscaled[] = { 0.0f, 1.0f };
				for ( float s : unscaled )
				{
					ent.v.scale = s;
					g_flRootScale = 1.0f;
					for ( int iBone = -1; iBone < g_pHdr->numbones; iBone += 7 )
					{
						Stub_SetupBones( NULL, frame, seq, angles, origin, controller, blending, iBone, &ent );
						memcpy( g_ref, g_bonetransform, sizeof( g_ref ) );
						memset( g_bonetransform, 0, sizeof( g_bonetransform ) );
						pInterface->SV_StudioSetupBones( NULL, frame, seq, angles, origin, controller, blending, iBone, &ent );
						CHECK( !memcmp( g_ref, g_bonetransform, sizeof( float ) * 12 * g_pHdr->numbones ),
							"scale %g seq %d frame %g bone %d: not the engine's bones", s, seq, frame, iBone );
					}
				}

				for ( float s : scales )
				{
					ent.v.scale = s;

					// the client's drawing: the root transform times s
					g_flRootScale = s;
					Stub_SetupBones( NULL, frame, seq, angles, origin, controller, blending, -1, &ent );
					memcpy( g_ref, g_bonetransform, sizeof( g_ref ) );
					g_flRootScale = 1.0f;

					// 2. the hitboxes
					pInterface->SV_StudioSetupBones( NULL, frame, seq, angles, origin, controller, blending, -1, &ent );
					for ( int h = 0; h < g_pHdr->numhitboxes; h++ )
					{
						const float (*bone)[4] = g_bonetransform[pbox[h].bone];
						float pos[3] = { bone[0][3], bone[1][3], bone[2][3] };
						float size = 0.0f;
						for ( int k = 0; k < 3; k++ )
							size += pbox[h].bbmax[k] - pbox[h].bbmin[k];
						for ( int k = 0; k < 3; k++ )
						{
							float n[3] = { bone[0][k], bone[1][k], bone[2][k] };
							float distMax = Dot3( n, pos ) + pbox[h].bbmax[k];
							float distMin = Dot3( n, pos ) + pbox[h].bbmin[k];
							// the drawn hitbox's faces: the corners with bbmax[k] / bbmin[k] on axis k
							for ( int c = 0; c < 8; c++ )
							{
								float corner[3], world[3];
								for ( int j = 0; j < 3; j++ )
									corner[j] = ( c >> j ) & 1 ? pbox[h].bbmax[j] : pbox[h].bbmin[j];
								Transform( corner, g_ref[pbox[h].bone], world );
								float d = Dot3( n, world ) - ( ( c >> k ) & 1 ? distMax : distMin );
								// the plane distance is in units of 1/s (the normal's length): world units = d * s
								CHECK( fabs( d * s ) < 0.002f * ( 1.0f + size * s ),
									"scale %g seq %d frame %g hitbox %d axis %d corner %d: %g units off the drawn face",
									s, seq, frame, h, k, c, d * s );
							}
							// a point just inside the max face and one just outside (0.5 units)
							float center[3], centerWorld[3];
							for ( int j = 0; j < 3; j++ )
								center[j] = ( pbox[h].bbmin[j] + pbox[h].bbmax[j] ) * 0.5f;
							Transform( center, g_ref[pbox[h].bone], centerWorld );
							float axisLen = sqrt( Dot3( n, n ) );
							float halfExtent = ( pbox[h].bbmax[k] - pbox[h].bbmin[k] ) * 0.5f * s;
							for ( int sign = -1; sign <= 1; sign += 2 )
							{
								float p[3];
								for ( int j = 0; j < 3; j++ )
									p[j] = centerWorld[j] + n[j] / axisLen * ( halfExtent + sign * 0.5f );
								bool inside = Dot3( n, p ) - distMax < 0;
								if ( halfExtent > 0.5f )
									CHECK( inside == ( sign < 0 ), "scale %g hitbox %d axis %d: point %s the face is %s",
										s, h, k, sign < 0 ? "inside" : "outside", inside ? "inside" : "outside" );
							}
						}
					}

					// 3. attachments and bone positions: one bone and its parents
					for ( int at = 0; at < g_pHdr->numattachments; at++ )
					{
						int bone = patt[at].bone;
						pInterface->SV_StudioSetupBones( NULL, frame, seq, angles, origin, controller, blending, bone, &ent );
						float ours[3], ref[3];
						Transform( patt[at].org, g_bonetransform[bone], ours );
						Transform( patt[at].org, g_ref[bone], ref );
						float err = sqrt( ( ours[0] - ref[0] ) * ( ours[0] - ref[0] ) + ( ours[1] - ref[1] ) * ( ours[1] - ref[1] ) +
							( ours[2] - ref[2] ) * ( ours[2] - ref[2] ) );
						CHECK( err < 0.01f, "scale %g seq %d frame %g attachment %d: %g units from the drawn one", s, seq, frame, at, err );
						for ( int b = bone; b != -1; b = pbones[b].parent )
						{
							float e = fabs( g_bonetransform[b][0][3] - g_ref[b][0][3] ) + fabs( g_bonetransform[b][1][3] - g_ref[b][1][3] ) +
								fabs( g_bonetransform[b][2][3] - g_ref[b][2][3] );
							CHECK( e < 0.01f, "scale %g attachment %d bone %d position: %g units off", s, at, b, e );
						}
					}
				}
			}
		}
		free( g_pHdr );
	}

	printf( "%d checks, %d failures\n", g_checks, g_failures );
	return g_failures ? 1 : 0;
}
