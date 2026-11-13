//========= SBO VGUI2: Create server window (the new menus) =========//
//
// Purpose: SBO's Create server window in the server browser's look
//          (docs/MENUS.md): mode tabs (Co-op, Bounty Killer, Battle Royale,
//          All maps), map picture tiles with a search and "Random map", and every settings.scr entry on the right.
//          Start does what HL25 GameUI's Create Server dialog does (CCreateMultiplayerGameDialog::OnOK):
//            1. the settings.scr values to the engine (CDescription::WriteToConfig) and settings.scr rewritten with
//               them as the new defaults (GameUI's format, sbo_script_file.h); the game DLL's cvars that do not
//               exist yet (the engine loads the DLL with the first map of the session) are set during that map's
//               load instead, before its entities spawn (VGui2_CreateServer_ApplyPending);
//            2. "disconnect\nsv_lan %d\nsv_use_steam_networking %d\nsetmaster enable\nmaxplayers %i\n
//               sv_password \"%s\"\nhostname \"%s\"\ncd fadeout\nmap %s\n" and "wait\nwait\n", with sv_lan = !steam,
//               maxplayers / sv_password / hostname from the settings (GameUI's fallbacks "32", "" and its
//               "<Steam name>'s Server" for hostname), the map (a random one of the list for "Random map").
//          The maps: maps/*.bsp of the game folder and its downloads (GameUI's GAME / GAMEDOWNLOAD path IDs).
//
//          Opened by the game menu's Create server entry ("engine gameui_createserver") when cl_experimental_menus is
//          1; with 0 the entry opens GameUI's dialog.
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_CREATE_SERVER_H
#define SBO_VGUI2_SBO_CREATE_SERVER_H

//! Registers gameui_createserver (HUD_Init time)
void VGui2_RegisterCreateServerCommand();
//! Create server's values for the game DLL's cvars, once the DLL that registers them has loaded (first map of the
//! session); called from the loading screen's paint and tick
void VGui2_CreateServer_ApplyPending();
//! Opens the window if the command asked for it (CGameUIViewport::Frame, one frame after the command)
void VGui2_CreateServer_Frame();
//! cl_experimental_menus changed (VGui2_MenusStyle_Frame): closes the window if it is open; true if it was
bool VGui2_CreateServer_CloseIfOpen();

#endif // SBO_VGUI2_SBO_CREATE_SERVER_H
