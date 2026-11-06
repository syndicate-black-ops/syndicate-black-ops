//========= SBO VGUI2 port (development branch chat-vgui2) =========//
// Ported from BugfixedHL-Rebased src/game/client/vgui/avatar_image.h (Valve code).
// Change: CAvatarImagePanel left out (the scoreboard only uses CAvatarImage). SDK world.
//=================================================================//
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#ifndef VGUI_AVATARIMAGE_H
#define VGUI_AVATARIMAGE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Image.h>
#include "steam/steam_api.h"
#include "tier1/utlmap.h"

// size of the friend background frame (see texture ico_friend_indicator_avatar)
#define FRIEND_ICON_SIZE_X (55)
#define FRIEND_ICON_SIZE_Y (34)

// offset of avatar within the friend icon
#define FRIEND_ICON_AVATAR_INDENT_X (22)
#define FRIEND_ICON_AVATAR_INDENT_Y (1)

// size of the standard avatar icon (unless override by SetAvatarSize)
#define DEFAULT_AVATAR_SIZE (32)

//=============================================================================
// HPE_CHANGE:
// [pfreese] Refactored these classes so that the CAvatarImage supports a
// default image to be used whenever the Steam avatar is not set or fails to
// be retrieved.
//=============================================================================

struct AvatarImagePair_t
{
	AvatarImagePair_t() { m_iAvatar = 0; }
	AvatarImagePair_t(CSteamID steamID, int av)
	{
		m_SteamID = steamID;
		m_iAvatar = av;
	}
	bool operator<(const AvatarImagePair_t &rhs) const
	{
		return m_SteamID.ConvertToUint64() < rhs.m_SteamID.ConvertToUint64() || (m_SteamID.ConvertToUint64() == rhs.m_SteamID.ConvertToUint64() && m_iAvatar < rhs.m_iAvatar);
	}

	CSteamID m_SteamID;
	int m_iAvatar;
};

//-----------------------------------------------------------------------------
// Purpose: avatar sizes, formerly used in ISteamFriends, but now only used in game code
//-----------------------------------------------------------------------------
enum EAvatarSize
{
	k_EAvatarSize32x32 = 0,
	k_EAvatarSize64x64 = 1,
	k_EAvatarSize184x184 = 2,
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CAvatarImage : public vgui2::IImage
{
public:
	CAvatarImage(void);

	// Call this to set the steam ID associated with the avatar
	//=============================================================================
	// HPE_BEGIN:
	// [tj] Added parameter to specify size. Default is 32x32.
	//=============================================================================
	bool SetAvatarSteamID(CSteamID steamIDUser, EAvatarSize avatarSize = k_EAvatarSize32x32);
	//=============================================================================
	// HPE_END
	//=============================================================================
	void UpdateFriendStatus(void);
	void ClearAvatarSteamID(void);

	// Call to Paint the image
	// Image will draw within the current panel context at the specified position
	virtual void Paint(void);

	// Set the position of the image
	virtual void SetPos(int x, int y)
	{
		m_nX = x;
		m_nY = y;
	}

	virtual void SetOffset(int x, int y)
	{
		m_offX = x;
		m_offY = y;
	}

	// Gets the size of the content
	virtual void GetContentSize(int &wide, int &tall)
	{
		wide = m_wide;
		tall = m_tall;
	}

	// Get the size the image will actually draw in (usually defaults to the content size)
	virtual void GetSize(int &wide, int &tall)
	{
		GetContentSize(wide, tall);
	}

	// Sets the size of the image
	virtual void SetSize(int wide, int tall);

	void SetAvatarSize(int wide, int tall);

	// Set the draw color
	virtual void SetColor(Color col)
	{
		m_Color = col;
	}

	bool IsValid() { return m_bValid; }
	//! SBO: the avatar's texture (loaded first if it is pending), -1 none: for drawing it in another shape (the GM
	//! players window's round map bubbles)
	int GetTextureID();
	int GetWide() { return m_wide; }
	int GetTall() { return m_tall; }
	int GetAvatarWide() { return m_avatarWide; }
	int GetAvatarTall() { return m_avatarTall; }

	//=============================================================================
	// HPE_BEGIN:
	//=============================================================================

	// [tj] simple setter for drawing friend icon
	void SetDrawFriend(bool drawFriend) { m_bDrawFriend = drawFriend; }

	// [pmf] specify the default (fallback) image
	void SetDefaultImage(vgui2::IImage *pImage) { m_pDefaultImage = pImage; }

	//=============================================================================
	// HPE_END
	//=============================================================================

	virtual bool Evict();
	virtual int GetNumFrames();
	virtual void SetFrame(int nFrame);
	virtual vgui2::HTexture GetID();
	virtual void SetRotation(int iRotation) { return; }

	virtual void SetSecondImage(vgui2::IImage *image) { m_SecondImage = image; }

protected:
	void InitFromRGBA(int iAvatar, const byte *rgba, int width, int height);

private:
	void LoadAvatarImage();

	Color m_Color;
	int m_iTextureID;
	int m_nX, m_nY;
	int m_wide, m_tall;
	int m_avatarWide, m_avatarTall;
	bool m_bValid;
	bool m_bFriend;
	bool m_bLoadPending;
	float m_fNextLoadTime; // used to throttle load attempts
	int m_offX = 0, m_offY = 0;

	EAvatarSize m_AvatarSize;
	CSteamID m_SteamID;
	vgui2::IImage *m_SecondImage = nullptr;

	//=============================================================================
	// HPE_BEGIN:
	//=============================================================================

	// [tj] Whether or not we should draw the friend icon
	bool m_bDrawFriend;

	// [pmf] image to use as a fallback when get from steam fails (or not called)
	vgui2::IImage *m_pDefaultImage;

	//=============================================================================
	// HPE_END
	//=============================================================================

	static CUtlMap<AvatarImagePair_t, int> s_AvatarImageCache;
	static bool m_sbInitializedAvatarCache;

#ifndef NO_STEAM
	CCallback<CAvatarImage, PersonaStateChange_t, false> m_sPersonaStateChangedCallback;
#endif

	void OnPersonaStateChanged(PersonaStateChange_t *info);
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------


#endif // VGUI_AVATARIMAGE_H
