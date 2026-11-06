//=========================================================
// SBO 2.0: a transparent studio model's attachments for the sprites that follow them (StudioModelRenderer.cpp)
//=========================================================

#if !defined( STUDIO_ATTACHMENTS_H )
#define STUDIO_ATTACHMENTS_H
#if defined( _WIN32 )
#pragma once
#endif

struct cl_entity_s;

// HUD_AddEntity: the attachments the renderer last computed for this entity, back in place of the origin the engine put
// there with the server's update (transparent studio models only)
void StudioAttachments_AddEntity( struct cl_entity_s *ent );

#endif // STUDIO_ATTACHMENTS_H
