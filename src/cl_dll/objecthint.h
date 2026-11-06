//=========================================================
// SBO 2.0 object hints, client side (objecthint.cpp, docs/OBJECT_HINTS.md; Featureful's ObjectHintManager): the
// "ObjectHint" message and the hint sprites, temporary entities that follow their object every frame.
//=========================================================

#if !defined( OBJECTHINT_CLIENT_H )
#define OBJECTHINT_CLIENT_H

void ObjectHint_Init( void );			// hooks the message, cl_objecthint
void ObjectHint_Reset( void );			// VidInit: the engine freed the temporary entities with the old map
void ObjectHint_CreateEntities( void );	// HUD_CreateEntities: moves the hints with their objects, ends old ones

#endif // OBJECTHINT_CLIENT_H
