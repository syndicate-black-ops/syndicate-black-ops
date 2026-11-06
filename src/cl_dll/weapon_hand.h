//=========================================================
// SBO 2.0: weapon hand switch (cl_righthand, as OpenAG: 0 = the weapon in the right hand, the default; 1 = in the left
// hand). The view model is mirrored in StudioSetupBones, shells eject to the left (EV_GetDefaultShellInfo), the
// view models' lettering reads the right way round through their "_lh" skin family, and the server reads the
// cvar from the userinfo to fire the RPG rocket from the left (CBasePlayer::SetPrefsFromUserinfo). docs/WEAPON_HAND.md.
//=========================================================

#if !defined( WEAPON_HAND_H )
#define WEAPON_HAND_H

#include "studio.h"

// CHud::Init: registers cl_righthand (archived, userinfo). The name must stay cl_righthand: the engine (HL25 hw.so
// IsFlippedViewModel / R_GLStudioDrawPoints) reverses the view model's face culling while a cvar of that name is > 0.
void WeaponHand_Init( void );

// The local player's own setting (the cvar): what the engine's face culling follows.
bool WeaponHand_LocalLeft( void );

// A player's setting as his userinfo carries it (every client gets every player's userinfo).
bool WeaponHand_PlayerLeft( int playerIndex );

// The view model drawn this frame is mirrored: hardware renderer only; in first-person spectating the watched
// player's setting, otherwise the local one.
bool WeaponHand_ViewLeft( void );

// The skin to draw a mirrored view model with: its "_lh" family (lettering mirrored back) when the model has one,
// else skin unchanged. A model has an "_lh" family when it has 2N skin families and family N + k differs from
// family k only in textures named like k's with "_lh" before the extension.
int WeaponHand_MirroredSkin( const studiohdr_t *hdr, int skin );

#endif // WEAPON_HAND_H
