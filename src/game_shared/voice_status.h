//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VOICE_STATUS_H
#define VOICE_STATUS_H
#pragma once



#include "voice_common.h"
#include "cl_entity.h"
#include "voice_banmgr.h"


class CVoiceStatus;


// This is provided by each mod to access data that may not be the same across mods.
class IVoiceStatusHelper
{
public:
	virtual					~IVoiceStatusHelper()	{}

	// Get RGB color for voice status text about this player.
	virtual void			GetPlayerTextColor(int entindex, int color[3]) = 0;

	// Force it to update the cursor state.
	virtual void			UpdateCursorState() = 0;

	// Return the height above the bottom that the voice ack icons should be drawn at.
	virtual int				GetAckIconHeight() = 0;

	// Return true if the voice manager is allowed to show speaker labels
	// (mods usually return false when the scoreboard is up).
	virtual bool			CanShowSpeakerLabels() = 0;
};

// SBO 2.0: the voice manager only (who talks, who is audible, the mute list, the icons above heads). The VGUI1
// speaker labels, the local "talking" icon and the VGUI1 scoreboard's icons are gone; the speaker list is VGUI2
// (cl_dll/vgui2/hud_voice_status.cpp), the scoreboard draws its own mute state.
class CVoiceStatus : public CHudBase
{
public:
				CVoiceStatus();
	virtual		~CVoiceStatus();

// CHudBase overrides.
public:
	
	// Initialize the cl_dll's voice manager.
	virtual int Init(
		IVoiceStatusHelper *m_pHelper,
		void **pParentPanel);
	
	// ackPosition is the bottom position of where CVoiceStatus will draw the voice acknowledgement labels.
	virtual int VidInit();


public:
	
	// Call from HUD_Frame each frame.
	void	Frame(double frametime);

	// Called when a player starts or stops talking.
	// entindex is -1 to represent the local client talking (before the data comes back from the server). 
	// When the server acknowledges that the local client is talking, then entindex will be gEngfuncs.GetLocalPlayer().
	// entindex is -2 to represent the local client's voice being acked by the server.
	void	UpdateSpeakerStatus(int entindex, qboolean bTalking);

	// Call from the HUD_CreateEntities function so it can add sprites above player heads.
	void	CreateEntities();

	// Called when the server registers a change to who this client can hear.
	void	HandleVoiceMaskMsg(int iSize, void *pbuf);

	// The server sends this message initially to tell the client to send their state.
	void	HandleReqStateMsg(int iSize, void *pbuf);


// Squelch mode functions.
public:

	// When you enter squelch mode, pass in 
	void	StartSquelchMode();
	void	StopSquelchMode();
	bool	IsInSquelchMode();

	// returns true if the target client has been banned
	// playerIndex is of range 1..maxplayers
	bool	IsPlayerBlocked(int iPlayerIndex);

	// returns false if the player can't hear the other client due to game rules (eg. the other team)
	bool    IsPlayerAudible(int iPlayerIndex);

	// blocks the target client from being heard
	void	SetPlayerBlockedState(int iPlayerIndex, bool blocked);

	// SBO: the player can be muted: not the local player, and he has a ban list key of his own (GetPlayerBanID)
	bool	CanPlayerBeBlocked(int iPlayerIndex);

	// SBO: the player's key in the ban list: his SteamID64 if he has one, else the engine's unique ID
	// (playerIndex 1..maxplayers). False for the local player and for players that can't be told apart from him.
	bool	GetPlayerBanID(int iPlayerIndex, char playerID[16]);

public:

	void			UpdateServerState(bool bForce);


public:

	float			m_LastUpdateServerState;		// Last time we called this function.
	int				m_bServerModEnable;				// What we've sent to the server about our "voice_modenable" cvar.

	void			**m_pParentPanel;		// the client viewport pointer: no speaker state before it exists
	CPlayerBitVec	m_VoicePlayers;		// Who is currently talking. Indexed by client index.
	
	// This is the gamerules-defined list of players that you can hear. It is based on what teams people are on 
	// and is totally separate from the ban list. Indexed by client index.
	CPlayerBitVec	m_AudiblePlayers;

	// Players who have spoken at least once in the game so far
	CPlayerBitVec	m_VoiceEnabledPlayers;	

	// This is who the server THINKS we have banned (it can become incorrect when a new player arrives on the server).
	// It is checked periodically, and the server is told to squelch or unsquelch the appropriate players.
	CPlayerBitVec	m_ServerBannedPlayers;

	cl_entity_s		m_VoiceHeadModels[VOICE_MAX_PLAYERS];			// These aren't necessarily in the order of players. They are just
																	// a place for it to put data in during CreateEntities.

	IVoiceStatusHelper	*m_pHelper;		// Each mod provides an implementation of this.

	
	// Squelch mode stuff.
	bool				m_bInSquelchMode;
	
	HSPRITE				m_VoiceHeadModel;		// Voice head model (goes above players who are speaking).
	float				m_VoiceHeadModelHeight;	// Height above their head to place the model.

	bool				m_bTalking;				// Set to true when the client thinks it's talking.
	bool				m_bServerAcked;			// Set to true when the server knows the client is talking.

public:
	
	CVoiceBanMgr		m_BanMgr;				// Tracks which users we have squelched and don't want to hear.

public:

	bool				m_bBanMgrInitialized;

	// Cache the game directory for use when we shut down
	char *				m_pchGameDir;
};


// Get the (global) voice manager. 
CVoiceStatus* GetClientVoiceMgr();


#endif // VOICE_STATUS_H
