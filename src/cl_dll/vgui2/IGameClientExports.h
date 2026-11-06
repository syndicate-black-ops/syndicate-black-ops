//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: Exports a set of functions for the GameUI interface to interact with
//          the game client (main menu "Mute players" dialog).
//
// SBO: copy of the Valve header (halflife public/cl_dll/IGameClientExports.h,
// BugfixedHL-Rebased src/public/cl_dll/IGameClientExports.h); hl1_source_sdk
// doesn't ship it. SDK world only: it uses tier1's IBaseInterface/InterfaceReg.
// The method order is the engine GameUI's vtable layout -- do not change it.
//
// $NoKeywords: $
//=============================================================================

#ifndef IGAMECLIENTEXPORTS_H
#define IGAMECLIENTEXPORTS_H
#ifdef _WIN32
#pragma once
#endif

#include <tier1/interface.h>

//-----------------------------------------------------------------------------
// Purpose: Exports a set of functions for the GameUI interface to interact with the game client
//-----------------------------------------------------------------------------
class IGameClientExports : public IBaseInterface
{
public:
	// returns the name of the server the user is connected to, if any
	virtual const char *GetServerHostName() = 0;

	// ingame voice manipulation
	virtual bool IsPlayerGameVoiceMuted(int playerIndex) = 0;
	virtual void MutePlayerGameVoice(int playerIndex) = 0;
	virtual void UnmutePlayerGameVoice(int playerIndex) = 0;
};

#define GAMECLIENTEXPORTS_INTERFACE_VERSION "GameClientExports001"

#endif // IGAMECLIENTEXPORTS_H
