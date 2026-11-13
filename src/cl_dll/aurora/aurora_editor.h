// SBO 2.0: the in-game particle editor's preview (aurora_editor.cpp), called by its windows
// (vgui2/sbo_particle_editor.cpp; docs/AURORA.md "Editor"). Plain types only: both the client's own code and the
// VGUI2 (Source SDK) code include it.
//
// The preview is one more Aurora system in the client's list (id AURORA_EDITOR_ID, never an entity's), built from
// the editor's script text as an env_particle's script is built (the same parser, simulation and drawing), at a
// place the editor sets: in front of the view, or a point picked on the screen. A new text builds it again (its
// particles start over). The client's own map changes clear it with the others; it is built again on the next frame.
#pragma once

#define AURORA_EDITOR_ID	-1

//! The script the preview runs (text: the whole .aur; name: shown in its problem lines). Null text: no preview.
void AuroraEditor_SetPreview( const char *text, const char *name );
//! Emitting (on) or not; Restart starts the effect over
void AuroraEditor_SetPreviewOn( bool on );
void AuroraEditor_Restart( void );
//! The emitter's turn (an env_particle's pitch and yaw: added to the script's spraypitch and sprayyaw)
void AuroraEditor_SetTurn( float pitch, float yaw );
//! Puts the emitter in front of the view (on the next frame: the view is known while it is drawn)
void AuroraEditor_PlaceInFront( void );
//! Puts the emitter where the line of sight through this point of the screen (0 .. 1 across and down) meets the
//! world, a little in front of the surface (on the next frame)
void AuroraEditor_PlaceAtScreen( float x, float y );
//! The problems the last build of the preview found (the game's console lines for the script): their count, and the
//! first one in first (without the "Aurora: <name>: " start)
int AuroraEditor_Problems( char *first, int firstSize );
//! Loads a sprite for the preview (the same cache the scripts use: one SPR_Load per sprite and map); false and the
//! reason in err when it can't
bool AuroraEditor_LoadSprite( const char *name, char *err, int errSize );
//! Particles alive in the preview now (for the window's status)
int AuroraEditor_PreviewParticles( void );

//! Aurora_DrawFrame (particlemgr.cpp), every frame before the systems are updated: builds and places the preview
void AuroraEditor_Frame( void );
