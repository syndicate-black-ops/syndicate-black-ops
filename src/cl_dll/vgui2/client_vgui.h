//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: IClientVGUI implementation (ported from BugfixedHL-Rebased
//          src/game/client/client_vgui.{h,cpp}).
//
// The engine gets this interface through the client's ClientFactory export
// (see client_vgui.cpp) and calls Initialize/Start/SetParent once at startup.
//
// As in BHL, UseVGUI1() returns 0: SBO 2.0 has no VGUI1 code (docs/dev/VGUI2_MIGRATION.md,
// "VGUI1 removal"). The panels sit on a minimal viewport (client_viewport.h); the menu
// stack, keys and cursor state are SBO's own code (cl_dll/viewport.cpp).
//=================================================================//

#ifndef SBO_VGUI2_CLIENT_VGUI_H
#define SBO_VGUI2_CLIENT_VGUI_H

#include <vgui/VGUI2.h>
#include <IClientVGUI.h>

#define VGUI2_ROOT_DIR "ui/"

class CClientVGUI : public IClientVGUI
{
public:
	virtual void Initialize(CreateInterfaceFn *pFactories, int iNumFactories);
	virtual void Start();
	virtual void SetParent(vgui2::VPANEL parent);
	virtual int UseVGUI1();
	virtual void HideScoreBoard();
	virtual void HideAllVGUIMenu();
	virtual void ActivateClientUI();
	virtual void HideClientUI();
	virtual void Shutdown();

private:
	//! Gets the base resolution for VGUI proportional scaling.
	static void GetProportionalBase(int &wide, int &tall);
};

#endif
