//=========================================================
// SBO 2.0: client-side prediction of moving brush entities (MOVETYPE_PUSH: doors, plats, trains, rotating, movewith
// children). docs/MOVER_PREDICTION.md.
//=========================================================

#if !defined( MOVER_PREDICTION_H )
#define MOVER_PREDICTION_H

struct playermove_s;
struct cl_entity_s;

// HUD_PlayerMove, client prediction only (server == 0), before PM_Move, in this order:
//  1. MoverPrediction_PlaceMovers: every pushing physent where the server will have it when this command runs;
//  2. MoveWith_PlayerMove: movewith's children composed on those physents (movewith_client.h);
//  3. MoverPrediction_MovePlayer: the player carried / pushed like SV_PushMove (a chain moves as one body).
// After PM_Move, MoverPrediction_PostMove notes the ground entity for the next command.
void MoverPrediction_PlaceMovers( struct playermove_s *ppmove );
void MoverPrediction_MovePlayer( struct playermove_s *ppmove );
void MoverPrediction_PostMove( struct playermove_s *ppmove );

// HUD_AddEntity: draws a mover the local player rides or is next to at its predicted pose
void MoverPrediction_AddEntity( int type, struct cl_entity_s *ent );
// HUD_CreateEntities, after the frame's entities are in: other players standing on such a mover are drawn moved with
// it
void MoverPrediction_CreateEntities( void );

// V_CalcRefdef: the view of a player riding a mover drawn at the drawing clock is moved with it
void MoverPrediction_ViewOrigin( float *origin );

// new map or server (VidInit): forget everything
void MoverPrediction_Reset( void );

#endif // MOVER_PREDICTION_H
