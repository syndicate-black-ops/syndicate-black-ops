//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: Minimal stand-in for BHL's ConVar (FCVAR_BHL_ARCHIVE) in the SDK world:
//          a plain archived engine cvar, registered through the bridge.
//          userinfo = also FCVAR_USERINFO: the value is a key of the player's
//          userinfo (key = cvar name), which the engine sends to every client.
//          Declare as a global, call Register() once from HUD_Init time
//          (the engine cvar system must be up).
//=================================================================//

#ifndef SBO_VGUI2_SBO_CVAR_H
#define SBO_VGUI2_SBO_CVAR_H

#include "sbo_bridge.h"

class CSboCvar
{
public:
	CSboCvar(const char *name, const char *defValue, bool userinfo = false)
	    : m_pszName(name)
	    , m_pszDefault(defValue)
	    , m_bUserInfo(userinfo)
	{
	}

	void Register() { m_hCvar = sbo::RegisterCvar(m_pszName, m_pszDefault, true, m_bUserInfo); }
	float GetFloat() const { return sbo::CvarGetFloat(m_hCvar); }
	int GetInt() const { return (int)GetFloat(); }
	bool GetBool() const { return GetFloat() != 0.0f; }
	const char *GetString() const { return sbo::CvarGetString(m_hCvar); }
	void SetValue(int value) const { sbo::CvarSetFloat(m_pszName, (float)value); }
	void SetValue(const char *value) const { sbo::CvarSetString(m_pszName, value); }
	const char *GetName() const { return m_pszName; }

private:
	const char *m_pszName;
	const char *m_pszDefault;
	bool m_bUserInfo;
	sbo::CvarHandle m_hCvar = nullptr;
};

#endif // SBO_VGUI2_SBO_CVAR_H
