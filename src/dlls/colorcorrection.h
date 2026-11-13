//=========================================================
// SBO 2.0 color_correction (point) and color_correction_volume (brush): Source-style color grading of the 3D view.
// docs/COLOR_CORRECTION.md (mapper guide); message format in game_shared/colorcorrection_shared.h.
//=========================================================

#ifndef SBO_DLLS_COLORCORRECTION_H
#define SBO_DLLS_COLORCORRECTION_H

class CBasePlayer;

// Every color_correction / color_correction_volume to this player: the client's HUD was (re)initialized (InitHUD,
// respawn, "fullupdate", a demo started mid-map). The first message tells the client to forget the ones it had.
void ColorCorr_SendAll( CBasePlayer *pPlayer );

#endif // SBO_DLLS_COLORCORRECTION_H
