//========= SBO VGUI2: game_timer HUD timer =========//
//
// Purpose: SBO side of the game_timer HUD timer (hud_game_timer.cpp): the state of SBO's HUD it asks for. Its own
//          file, so the timer doesn't touch the other bridge files.
//
// SBO world: compiled with SBO's client include paths (SDK 2.3) + sbo_compat.h. No Source SDK headers.
//=================================================================//

#include "hud.h"
#include "cl_util.h"

// ammo.cpp: NULL = the weapon selection is off, 1 = just the slot bar, else the selected weapon
extern WEAPON *gpActiveSel;

namespace sbo
{
// Source's timer hides while the weapon selection is visible (CHudGenericGameTimer::ShouldDraw)
bool IsWeaponSelectionOpen()
{
	return gpActiveSel != NULL;
}
} // namespace sbo
