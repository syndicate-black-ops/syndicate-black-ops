//=========================================================
// Entity parenting: the "movewith" key (Spirit of Half-Life) and its alias "parent" (XashXT / PrimeXT).
// Mapper guide and design: docs/MOVEWITH.md. Server side: movewith.cpp; client side: cl_dll/movewith_client.cpp.
//=========================================================
#ifndef MOVEWITH_H
#define MOVEWITH_H

class CBaseEntity;
struct entity_state_s;

// "parentflags" (XashXT's bits): the child keeps its own world value of that axis
#define PARENT_FROZEN_POS_X		( 1 << 1 )
#define PARENT_FROZEN_POS_Y		( 1 << 2 )
#define PARENT_FROZEN_POS_Z		( 1 << 3 )
#define PARENT_FROZEN_ROT_X		( 1 << 4 )	// pitch
#define PARENT_FROZEN_ROT_Y		( 1 << 5 )	// yaw
#define PARENT_FROZEN_ROT_Z		( 1 << 6 )	// roll

// DispatchKeyValue: "movewith", "parent", "parentflags" on any entity. TRUE if handled.
BOOL MoveWith_KeyValue( edict_t *pent, KeyValueData *pkvd );

// DispatchSpawn, around the entity's Spawn(): the pose it has in the editor
void MoveWith_PreSpawn( edict_t *pent );
void MoveWith_PostSpawn( edict_t *pent );

void MoveWith_ServerActivate( void );	// before the entities' Activate: links, from the editor positions
void MoveWith_PostActivate( void );		// after Activate: puts every child at its place
void MoveWith_ServerDeactivate( void );
void MoveWith_StartFrame( void );		// every server frame, before the engine moves anything
void MoveWith_CmdStart( edict_t *pPlayer );	// before a player's usercmd runs (CmdStart): its ground back
#ifdef SBO_TEST_COMMANDS
void MoveWith_TraceCmd( edict_t *pPlayer, int bEnd, int msec );	// test builds: a traced player's usercmd (MWC lines)
void MoveWith_TraceTouch( edict_t *pTouched, edict_t *pOther );	// test builds: a traced player's touches (MWX lines)
#endif

// SetObjectCollisionBox (the engine's pfnSetAbsBox): a turned brush child's absmin / absmax from its turned bounds.
// FALSE: not one (the stock box applies).
BOOL MoveWith_CollisionBox( entvars_t *pev );

// DispatchBlocked: a child forwards its block to the top of its chain. TRUE if handled.
BOOL MoveWith_Blocked( CBaseEntity *pEntity, CBaseEntity *pOther );

// AddToFullPack: a child the client places on its parent gets its local pose and iuser4
void MoveWith_AddToFullPack( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, unsigned char *pSet );
void MoveWith_CreateBaseline( struct entity_state_s *baseline, edict_t *ent );	// a placed child's local pose and link

// ambient_generic: the entity a child's sound plays on so that it moves with it on the clients (its parent, when the
// clients receive it), or NULL. HasParentKey: the map gave it a parent (it may not be linked yet).
edict_t *MoveWith_SoundEntity( edict_t *pent );
BOOL MoveWith_HasParentKey( edict_t *pent );
BOOL MoveWith_IsChild( edict_t *pent );	// linked to a parent now (mover prediction: its velocity is movewith's steering)
void MoveWith_ClientHUDInit( void );	// a client's HUD was (re)initialised: catch up the looping child sounds
void AmbientGeneric_MoveWithCatchUp( CBaseEntity *pAmbient );	// sound.cpp: its looping sound as a volume change
// where a child ambient's sound started (sound.cpp): its changes and its stop go to that entity even when it is no
// longer sent or the ambient lost its parent; a sound started on an entity being removed stops with it
void MoveWith_SoundStarted( edict_t *pAmbient, edict_t *pSoundEnt, const char *szSample );
edict_t *MoveWith_SoundStartedOn( edict_t *pAmbient, const char *szSample );	// NULL: none, or that entity is gone
void MoveWith_SoundStopped( edict_t *pAmbient );
void MoveWith_EntityRemoved( edict_t *pent );	// CBaseEntity::UpdateOnRemove

// trigger_changeparent and code: pParent NULL = no parent. The entity keeps its world pose. FALSE if refused.
BOOL MoveWith_SetParent( CBaseEntity *pChild, CBaseEntity *pParent, int iAttachment );

// A child's pose and motion as other entities set them (trigger_motion, motion_manager, motion_thread,
// trigger_changevalue). For an entity that is not a child these are the plain entvars writes. For a child: a place
// or angles in the world are where it goes now, and it keeps that pose on its parent; a velocity (world directions)
// or an angular velocity is its own movement on top of its parent's (Spirit of Half-Life). IsChild: linked, with its
// parent here, and not a sprite on an attachment (the engine places that one; it gets the plain writes).
BOOL MoveWith_IsChild( CBaseEntity *pEntity );
void MoveWith_AssignOrigin( CBaseEntity *pEntity, const Vector &vecOrigin );
void MoveWith_AssignAngles( CBaseEntity *pEntity, const Vector &vecAngles );
Vector MoveWith_GetVelocity( CBaseEntity *pEntity );
void MoveWith_SetVelocity( CBaseEntity *pEntity, const Vector &vecVelocity );
Vector MoveWith_GetAvelocity( CBaseEntity *pEntity );
void MoveWith_SetAvelocity( CBaseEntity *pEntity, const Vector &vecAvelocity );

// point_worldtext / point-mode vgui_screen with a parent the clients receive: the parent (as iuser4: index and
// attachment) and the local pose the clients place it with on the parent's drawn pose. FALSE: drawn where it is.
BOOL MoveWith_ClientLink( CBaseEntity *pEntity, int *piParent, Vector *pLocalOrigin, Vector *pLocalAngles );

// KeyValue_DispatchRuntime (trigger_changevalue, trigger_copyvalue): before the key is written, "origin", "angles",
// "velocity" and "avelocity" of a child go through the calls above (TRUE: done); after it, a new "movewith",
// "parent" or "parentflags" re-links the entity as trigger_changeparent does
BOOL MoveWith_RuntimeKeyValue( edict_t *pent, const char *szKeyName, const char *szValue );
void MoveWith_RuntimeKeyChanged( edict_t *pent, const char *szKeyName );
// CBaseEntity::GetKeyValue: a child's "velocity" and "avelocity" are its own (relative to its parent). TRUE: set.
BOOL MoveWith_GetKeyValue( CBaseEntity *pEntity, const char *szKeyName, Vector &vecValue );

// Own movement of a child mover (CBaseToggle::LinearMove / AngularMove): its pose in the frame of its parent as
// placed. GetOwnPose returns FALSE (and nothing) for an entity that is not a moving child.
BOOL MoveWith_GetOwnPose( CBaseEntity *pEntity, Vector *pOrigin, Vector *pAngles );
void MoveWith_SetOwnVelocity( CBaseEntity *pEntity, const Vector &vecVelocity );
void MoveWith_SetOwnAvelocity( CBaseEntity *pEntity, const Vector &vecAvelocity );
void MoveWith_SetOwnOrigin( CBaseEntity *pEntity, const Vector &vecOrigin );
void MoveWith_SetOwnAngles( CBaseEntity *pEntity, const Vector &vecAngles );
Vector MoveWith_GetOwnAvelocity( CBaseEntity *pEntity );	// pev->avelocity unless a moving child
Vector MoveWith_GetOwnVelocity( CBaseEntity *pEntity );		// pev->velocity unless a moving child

// func_tank on a moving parent: a world direction in the entity's own frame (FALSE, and *pOwn untouched, for an entity
// that is not a child), and a world point in its parent's frame (unchanged for an entity that is not a child)
BOOL MoveWith_WorldToOwnDirection( CBaseEntity *pEntity, const Vector &vecWorld, Vector *pOwn );
Vector MoveWith_ToParentFrame( CBaseEntity *pEntity, const Vector &vecWorld );

#endif // MOVEWITH_H
