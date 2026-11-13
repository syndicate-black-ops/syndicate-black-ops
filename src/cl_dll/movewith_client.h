//=========================================================
// Entity parenting (movewith), client side. movewith_client.cpp, docs/MOVEWITH.md.
//=========================================================
#ifndef MOVEWITH_CLIENT_H
#define MOVEWITH_CLIENT_H

struct cl_entity_s;

int MoveWith_AddEntity( int type, struct cl_entity_s *ent );	// HUD_AddEntity: 0 = don't draw it this frame
void MoveWith_CreateEntities( void );							// HUD_CreateEntities
void MoveWith_VidInit( void );
bool MoveWith_IsPlaced( const struct cl_entity_s *ent );			// placed on its parent for this frame's drawing
#ifdef SBO_TEST_COMMANDS
void MoveWith_TraceDraw( struct cl_entity_s *ent, float (*transform)[4] );	// test builds: where the studio renderer draws it (MWD)
#endif
struct playermove_s;
void MoveWith_PlayerMove( struct playermove_s *ppmove );		// HUD_PlayerMove (client prediction): placed solid children
int MoveWith_ClientParent( struct cl_entity_s *ent );			// a client-placed child's parent index, else 0
#ifdef SBO_TEST_COMMANDS
void MoveWith_TraceView( const float *simorg, const float *vieworg );	// V_CalcRefdef, test builds: "MWV" lines
#endif
void MoveWith_EventPhysents( void );
// point_worldtext / point-mode vgui_screen with a parent (their own messages carry it): the world pose on the parent's
// drawn pose this frame. iParent as iuser4 (index, attachment); angles in: the last pose (near), out: the new one.
// false: the parent isn't in this frame (keep the last pose).
bool MoveWith_PlaceOnParent( int iParent, const float *localOrigin, const float *localAngles, float *origin, float *angles );							// client event traces, after EV_SetSolidPlayers									// map start

#endif // MOVEWITH_CLIENT_H
