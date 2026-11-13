//========= SBO VGUI2: trigger_objective HUD list =========//
//
// Purpose: the mission objectives of the map entity trigger_objective (server: src/dlls/trigger_objective.cpp, the
//          Objective message; list logic: src/cl_dll/objective_core.cpp; docs/OBJECTIVES.md):
//            * the panel at the top left: "OBJECTIVES", how many are active, objectives with a triangle, the
//              text and a counter ("6/10"); a group shows its done parts ("1/3") and its parts indented under it; a
//              completed line is gray, ticked and struck through. hud_objectives 1 short (at most 5 lines, the newest
//              objectives first, "+N more"; completed lines fade out), 2 full (all active objectives with their
//              parts, then the completed ones), 0 hidden; the command "objectives" (key H) cycles 1 -> 2 -> 0.
//            * the NEW OBJECTIVE / OBJECTIVE COMPLETE notices under the panel (where it is when hidden): slide in,
//              hold, fade out (about 3 s), with the tutor's message sound; not for silent objectives; shown in every
//              mode. They wait while the HUD is hidden (a camera, hud_draw 0).
//            * the scoreboard's one line under the players (Objectives_PaintSummary). The panel and the notices are
//              not drawn while the scoreboard or a text window (MOTD, map briefing) is open; the notices wait.
//          Look: a dark translucent box, teal heading and markers, an amber notice. Sizes in 640x480
//          units times screen height / 480, as the kill feed; no HUD sprites.
//
// SDK world (hl1_source_sdk headers); SBO data through sbo_bridge.h.
//=================================================================//

#ifndef SBO_VGUI2_HUD_OBJECTIVES_H
#define SBO_VGUI2_HUD_OBJECTIVES_H

#include <vgui_controls/Panel.h>

class CHudObjectives : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CHudObjectives, vgui2::Panel);

public:
	static CHudObjectives *Create(vgui2::Panel *pParent);
	static CHudObjectives *Get() { return s_pInstance; }

	//! InitHUD (new map / connection): no objectives.
	void Reset();
	//! The Objective message.
	void OnMessage(const unsigned char *data, int size);

	virtual void OnThink() override;
	virtual void PaintBackground() override;
	virtual void Paint() override;

private:
	CHudObjectives(vgui2::Panel *pParent);

	static CHudObjectives *s_pInstance;

	double m_flLastThinkTime = 0;
	float m_flListY = -1.0f; //!< the list's top, moving after the kill feed's rows (screen pixels)
};

//! The scoreboard's one line under the players ("OBJECTIVES  3/9 - Next: ... - +2 more"): its height, 0: nothing.
int Objectives_SummaryTall();
//! hint: 0 none, 1 "Click for all objectives", 2 "Click to close" (the scoreboard has the mouse)
void Objectives_PaintSummary(int x, int y, int wide, int hint);
//! The full list (as the panel's full mode) in a box whose bottom is at bottom, at most maxTall high ("+N more"); for
//! the scoreboard's expanded line. Returns the box's top.
int Objectives_PaintFullListUp(int x, int bottom, int wide, int maxTall);
//! Changes with every change of the list (a window showing the full list lays itself out again).
int Objectives_Revision();

#endif // SBO_VGUI2_HUD_OBJECTIVES_H
