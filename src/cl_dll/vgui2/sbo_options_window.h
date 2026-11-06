//========= SBO VGUI2: Options window (the new menus) =========//
//
// Purpose: SBO's Options and Advanced windows in the server browser's look
//          (docs/MENUS.md). Options: GameUI's Options pages (Multiplayer,
//          Keyboard, Mouse, Audio, Video, Voice, Content). Advanced (its own window): SBO's Advanced
//          dialog's tabs (General, Chat, Version). Every setting is read and written
//          the way HL25's GameUI (or SBO's Advanced dialog) does it, so config.cfg, user.scr and the engine's other
//          stores end up the same (the inventory of GameUI's pages is in docs/MENUS.md).
//
//          Opened by the game menu's Options entry ("engine gameui_options") and Advanced entry
//          ("engine gameui_cl_open_adv_options") when cl_experimental_menus is 1; with 0 those entries open GameUI's
//          Options dialog and SBO's old Advanced dialog as before.
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_OPTIONS_WINDOW_H
#define SBO_VGUI2_SBO_OPTIONS_WINDOW_H

//! Registers gameui_options (HUD_Init time)
void VGui2_RegisterOptionsWindowCommand();
//! Opens the window on its tab if a command asked for it (CGameUIViewport::Frame, one frame after the command)
void VGui2_OptionsWindow_Frame();
//! Opens SBO's new-style Advanced window (General, Chat, Version); from gameui_cl_open_adv_options
void VGui2_OpenAdvancedWindow();
//! cl_experimental_menus changed (Apply, OK or the console): every open menu window switches style at once. SBO's
//! open windows close and the same window opens in the other style on the next frame; GameUI's own Options, Find
//! servers and Create server dialogs close for SBO's. The tab is kept where the other window has it. The joining
//! screen follows at the next connect. CGameUIViewport::Frame, before the windows' open hooks.
void VGui2_MenusStyle_Frame();

#endif // SBO_VGUI2_SBO_OPTIONS_WINDOW_H
