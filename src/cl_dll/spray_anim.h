//=========================================================
// SBO 2.0: animated sprays (spray_anim.cpp). docs/ANIMATED_SPRAYS.md.
//=========================================================

#if !defined( SPRAY_ANIM_H )
#define SPRAY_ANIM_H

// Once per frame with a GL context (HUD_DrawNormalTriangles): puts the current frame of each player's animated spray
// into the texture the engine draws that player's sprays with. Nothing for still sprays or without OpenGL.
void SprayAnim_Frame( void );

#endif // SPRAY_ANIM_H
