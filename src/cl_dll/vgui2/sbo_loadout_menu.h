//========= SBO VGUI2: the new loadout menu =========//
//
// Purpose: the loadout (briefing / equipment) menu in the new in-game look, shown in place of CSboBriefMenu when
//          "New in-game menus" is on (cl_new_ingame_menus 1; sbo_ui_style.h SboNewIngameMenus). Spec:
//          docs/LOADOUT_MENU.md.
//
//          Only the look is new. It opens and closes as CSboBriefMenu (server menu 10, "ShowVGUI 10" from the team
//          menu's Go!; the same proxy on the viewport's menu stack), keeps the choices in the same place
//          (sbo::Get/SetBriefChoice: TeamFortressViewport::m_iBlade ...) and sends the same commands on DEPLOY:
//          "blade n", "light n", "heavy n", "item n", "cyberware n", "armor n", "malefemale n", then HideTopMenu.
//          Cancel and the close button do what CSboBriefMenu's Cancel does.
//
//          Window: the seven slots on the left (keys 1-7 through the proxy's SlotInput), the slot's items as cards
//          (gfx/vgui/640_<item>.tga), the item's details on the right (the whole picture, a click shows it large;
//          name, summary, stat bars, attack lines, the original #SBO_Item_<item> text in INFO), the seven choices
//          at the bottom, Cancel and DEPLOY. Named loadouts (Save as / Rename / Delete) and the last loadout are kept
//          in loadouts.txt in the mod folder; the dice picks a random loadout. The header shows the map, mode and
//          round and the round status from the server ("RoundStatus": NEXT ROUND IN m:ss, waiting for players, a
//          round in progress).
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_LOADOUT_MENU_H
#define SBO_VGUI2_SBO_LOADOUT_MENU_H

//! The new menu instead of CSboBriefMenu (cl_new_ingame_menus 1)
bool VGui2_LoadoutMenu_Wanted();
void VGui2_LoadoutMenu_Show();
void VGui2_LoadoutMenu_Hide();
//! Keys 1-7 while the menu is up: that slot. True when used.
bool VGui2_LoadoutMenu_SlotInput(int iSlot);

#endif // SBO_VGUI2_SBO_LOADOUT_MENU_H
