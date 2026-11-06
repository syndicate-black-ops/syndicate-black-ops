//=========================================================
// SBO 2.0: env_mirror, client side (Spirit of Half-Life 1.8, G-Cont). The server sends each active mirror in the
// "Mirror" message (dlls/env_mirror.cpp); the studio renderer draws the models near one a second time, flipped
// across its plane (StudioModelRenderer.cpp). docs/ENV_MIRROR.md.
//=========================================================

#if !defined( MIRROR_H )
#define MIRROR_H

#define MAX_MIRRORS	32
#define MIRROR_EDGE_MARGIN	64	// units around the mirror brush a reflected origin may be seen through (models' size)	// Spirit's limit; more are ignored (with a console message)

struct sbo_mirror_t
{
	int		id;				// the server's entity index
	int		drawPlayers;	// "Draw Player": players are reflected too
	int		axis;			// 0 X, 1 Y, 2 Z (a floor)
	int		side;			// 1: reflects for the positive side of the axis, -1: the negative side, 0: both
	float	mins[3];		// the mirror brush: a reflection is only drawn where it is seen through it
	float	maxs[3];
	float	center[3];		// the radius is measured from here
	float	plane;			// the reflection plane: axis coordinate
	float	radius;
};

void Mirror_Init( void );		// hooks the message
void Mirror_Reset( void );		// new map or server (VidInit, InitHUD): no mirror until the server sends them

int Mirror_Count( void );
const sbo_mirror_t *Mirror_Get( int i );

// Whether a model at origin is reflected in the mirror for a view at vieworg: within the radius, the view on a
// reflecting side, the model on the same side (no ghosts behind the mirror), and its reflection seen through the
// mirror brush (MIRROR_EDGE_MARGIN around it, for the model's size)
bool Mirror_Reflects( const sbo_mirror_t *pMirror, const float *origin, const float *vieworg );

// The first-person reflection of the local player (or the player watched in eye): the engine draws no model for
// the player whose eyes are the view, so HUD_CreateEntities adds a copy of his entity, which the studio renderer
// draws only as reflections. Returns the player's index (1..) when ent is that copy this frame, else 0.
void Mirror_CreateEntities( void );
int Mirror_PlayerCopyIndex( const struct cl_entity_s *ent );

// The local player's predicted position and view angles of this frame (V_CalcNormalRefdef, while playing): the
// engine leaves his entity at the last server update in first person, so his reflection would move in steps.
void Mirror_SetLocalView( const float *origin, const float *viewangles );
// For the first-person copy of the local player: the origin and the model angles (pitch as the engine gives a
// player model: -view pitch / 3) to draw it with. 0 for other players (in eye: the engine interpolates them).
int Mirror_PlayerCopyPredicted( float *origin, float *angles );

// Sprite entities (HUD_AddEntity notes them, HUD_CreateEntities adds the copies) and sprite temp entities
// (HUD_TempEntUpdate, after the engine's): per-frame visible-entity copies at the mirrored positions
void Mirror_AddEntity( int type, struct cl_entity_s *ent );
void Mirror_TempEntities( struct tempent_s *pActive, double client_time );

// Effects drawn by the client with the triangle API: the hook point (usage in mirror.cpp; Aurora's env_particle
// calls it when branch feat/aurora-particles is merged)
#define MIRROR_EFFECT_MAX	8
void Mirror_ReflectPoint( const sbo_mirror_t *pMirror, const float *in, float *out );
const float *Mirror_EffectViewOrigin( void );
int Mirror_EffectMirrors( int *list );
void Mirror_EffectBegin( const sbo_mirror_t *pMirror );	// clip plane at the mirror, face culling off
void Mirror_EffectEnd( void );

#endif // MIRROR_H
