//=========================================================
// SBO 2.0: killfeed icons set by server plugins, client side (killfeed_icons.cpp, docs/KILLFEED_ICONS.md): the
// "KfIcon" message adds d_<code> to the HUD sprite list (CHud::AddExtraSprite), where both kill feeds find it.
//=========================================================

#if !defined( KILLFEED_ICONS_CLIENT_H )
#define KILLFEED_ICONS_CLIENT_H

void KillFeedIcons_Init( void );	// hooks the message
void KillFeedIcons_Reset( void );	// VidInit: none on a new map or server (the server sends its table)

#endif // KILLFEED_ICONS_CLIENT_H
