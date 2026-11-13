//=========================================================
// SBO 2.0: the local player's flashlight light, placed by client.dll (flashlight.cpp)
//=========================================================

#pragma once

struct entity_state_s;

// HUD_ProcessPlayerState: the local player's flashlight bit is kept here and taken off his state, so the engine
// doesn't place the light itself
void Flashlight_ProcessPlayerState( struct entity_state_s *dst );

// HUD_CreateEntities: the light where the view meets the world as it is drawn this frame
void Flashlight_CreateEntities( void );
