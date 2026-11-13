/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//			
//  hud.h
//
// class CHud declaration
//
// CHud handles the message, calculation, and drawing the HUD
//


#define RGB_YELLOWISH 0x0042F6C9 // SBO: 66,246,201 (SDK: 0x00FFA000)
#define RGB_REDISH 0x00FF1010 //255,160,0
#define RGB_GREENISH 0x0042F6C9 // SBO (SDK: 0x0000A000)
#define RGB_BLUEISH 0x0032B274 // SBO: 50,178,116 (health, battery, flashlight, ammohistory, spectator)

#include "wrect.h"
#include "cl_dll.h"
#include "ammo.h"

#define DHN_DRAWZERO 1
#define DHN_2DIGITS  2
#define DHN_3DIGITS  4
#define DHN_4DIGITS  8		// SBO
#define DHN_5DIGITS  16		// SBO
#define MIN_ALPHA	 100	

#define		HUDELEM_ACTIVE	1

typedef struct {
	int x, y;
} POSITION;

enum 
{ 
	MAX_PLAYERS = 64,
	MAX_TEAMS = 64,
	MAX_TEAM_NAME = 16,
};

typedef struct {
	unsigned char r,g,b,a;
} RGBA;

typedef struct cvar_s cvar_t;


#define HUD_ACTIVE	1
#define HUD_INTERMISSION 2

#define MAX_PLAYER_NAME_LENGTH		32

// SBO 2.0: 3072 (was 1536), BugfixedHL-Rebased's MAX_UNICODE_MOTD_LENGTH; must match the server's MAX_MOTD_LENGTH
// (dlls/multiplay_gamerules.cpp). m_szMOTD keeps the first MAX_MOTD_LENGTH - 1 bytes (MsgFunc_MOTD strncat).
#define	MAX_MOTD_LENGTH				3072

//
//-----------------------------------------------------
//
class CHudBase
{
public:
	POSITION  m_pos;
	int   m_type;
	int	  m_iFlags; // active, moving, 
	virtual		~CHudBase() {}
	virtual int Init( void ) {return 0;}
	virtual int VidInit( void ) {return 0;}
	virtual int Draw(float flTime) {return 0;}
	virtual void Think(void) {return;}
	virtual void Reset(void) {return;}
	virtual void InitHUDData( void ) {}		// called every time a server is connected to

};

struct HUDLIST {
	CHudBase	*p;
	HUDLIST		*pNext;
};



//
//-----------------------------------------------------
//
#include "../game_shared/voice_status.h"
#include "hud_spectator.h"


//
//-----------------------------------------------------
//
class CHudAmmo: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	void Think(void);
	void Reset(void);
	int DrawWList(float flTime);
	int MsgFunc_CurWeapon(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_WeaponList(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_AmmoX(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_AmmoPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_WeapPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_ItemPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_HideWeapon( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_WeaponsHigh( const char *pszName, int iSize, void *pbuf );	// SBO 2.1: weapon ids 32..63

	void SlotInput( int iSlot );
	void _cdecl UserCmd_Slot1( void );
	void _cdecl UserCmd_Slot2( void );
	void _cdecl UserCmd_Slot3( void );
	void _cdecl UserCmd_Slot4( void );
	void _cdecl UserCmd_Slot5( void );
	void _cdecl UserCmd_Slot6( void );
	void _cdecl UserCmd_Slot7( void );
	void _cdecl UserCmd_Slot8( void );
	void _cdecl UserCmd_Slot9( void );
	void _cdecl UserCmd_Slot10( void );
	void _cdecl UserCmd_Close( void );
	void _cdecl UserCmd_NextWeapon( void );
	void _cdecl UserCmd_PrevWeapon( void );
	void _cdecl UserCmd_LastWeapon( void );
	WEAPON *GetCurWeapon( void );	// SBO
	void UpdateCrosshair( void );			// BugfixedHL-Rebased: the one place that chooses the crosshair
	void SetWeaponCrosshair( void );		// the current weapon's crosshair (none without a weapon)

private:
	float m_fFade;
	RGBA  m_rgba;
	WEAPON *m_pWeapon;
	int	m_fOnTarget;			// BugfixedHL-Rebased: CurWeapon state 2+ (auto-aim crosshair)
	int	m_iLastFOV;				// BugfixedHL-Rebased: gHUD.m_iFOV the crosshair was chosen for
	int	m_iLastObserverMode;	// g_iUser1 the crosshair was chosen for
	int	m_HUD_bucket0;
	int	m_HUD_buckets[ MAX_WEAPON_SLOTS ];	// SBO 2.1: the top row sprite of each slot (bucket1 .. bucket9, bucket0 for 10); -1: none
	int m_HUD_selection;

};

//
//-----------------------------------------------------
//

class CHudAmmoSecondary: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw(float flTime);

	int MsgFunc_SecAmmoVal( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_SecAmmoIcon( const char *pszName, int iSize, void *pbuf );

private:
	enum {
		MAX_SEC_AMMO_VALUES = 4
	};

	int m_HUD_ammoicon; // sprite indices
	int m_iAmmoAmounts[MAX_SEC_AMMO_VALUES];
	float m_fFade;
};


#include "health.h"


#define FADE_TIME 100


//
//-----------------------------------------------------
//
class CHudGeiger: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	int MsgFunc_Geiger(const char *pszName, int iSize, void *pbuf);
	
private:
	int m_iGeigerRange;

};

//
//-----------------------------------------------------
//
class CHudTrain: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	int MsgFunc_Train(const char *pszName, int iSize, void *pbuf);

private:
	HSPRITE m_hSprite;
	int m_iPos;

};

//
//-----------------------------------------------------
//
// REMOVED: Vgui has replaced this.
//
/*
class CHudMOTD : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );

	int MsgFunc_MOTD( const char *pszName, int iSize, void *pbuf );

protected:
	static int MOTD_DISPLAY_TIME;
	char m_szMOTD[ MAX_MOTD_LENGTH ];
	float m_flActiveRemaining;
	int m_iLines;
};
*/

//
//-----------------------------------------------------
//
class CHudStatusBar : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );
	void ParseStatusString( int line_num );
	int DrawStatusLine( int line_num, int x, int y, bool bDraw );

	int MsgFunc_StatusText( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_StatusValue( const char *pszName, int iSize, void *pbuf );

protected:
	enum { 
		MAX_STATUSTEXT_LENGTH = 128,
		MAX_STATUSBAR_VALUES = 8,
		MAX_STATUSBAR_LINES = 2,
	};

	char m_szStatusText[MAX_STATUSBAR_LINES][MAX_STATUSTEXT_LENGTH];  // a text string describing how the status bar is to be drawn
	char m_szStatusBar[MAX_STATUSBAR_LINES][MAX_STATUSTEXT_LENGTH];	// the constructed bar that is drawn
	int m_iStatusValues[MAX_STATUSBAR_VALUES];  // an array of values for use in the status bar

	int m_bReparseString; // set to TRUE whenever the m_szStatusBar needs to be recalculated

	// an array of colors...one color for each line
	float *m_pflNameColors[MAX_STATUSBAR_LINES];

	// SBO: the player whose name each char of m_szStatusBar is part of (0 = not a name). A name is drawn as the chat
	// and kill feed draw it: his name color (cvar namecolor) and the codes typed in it (hud_colortext), else his
	// team color; the rest of the line keeps the line's color (m_pflNameColors).
	unsigned char m_iNameClients[MAX_STATUSBAR_LINES][MAX_STATUSTEXT_LENGTH];
	bool m_bNameColorCodes[MAX_STATUSBAR_LINES];	// the names' color codes are drawn as colors (hud_colortext 1)
	bool m_bHasNames;	// a line shows a player name: parsed again every frame (name, name color, hud_colortext)
};

//
//-----------------------------------------------------
//
// REMOVED: Vgui has replaced this.
//
/*
class CHudScoreboard: public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	int Draw( float flTime );
	int DrawPlayers( int xoffset, float listslot, int nameoffset = 0, char *team = NULL ); // returns the ypos where it finishes drawing
	void UserCmd_ShowScores( void );
	void UserCmd_HideScores( void );
	int MsgFunc_ScoreInfo( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_TeamInfo( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_TeamScore( const char *pszName, int iSize, void *pbuf );
	void DeathMsg( int killer, int victim );

	int m_iNumTeams;

	int m_iLastKilledBy;
	int m_fLastKillTime;
	int m_iPlayerNum;
	int m_iShowscoresHeld;

	void GetAllPlayersInfo( void );
private:
	struct cvar_s *cl_showpacketloss;

};
*/

struct extra_player_info_t 
{
	short frags;
	short deaths;
	short playerclass;
	short teamnumber;
	char teamname[MAX_TEAM_NAME];
};

struct team_info_t 
{
	char name[MAX_TEAM_NAME];
	short frags;
	short deaths;
	short ping;
	short packetloss;
	short ownteam;
	short players;
	int already_drawn;
	int scores_overriden;
	int teamnumber;
};

extern hud_player_info_t	g_PlayerInfoList[MAX_PLAYERS+1];	   // player info from the engine
extern extra_player_info_t  g_PlayerExtraInfo[MAX_PLAYERS+1];   // additional player info sent directly to the client dll
extern team_info_t			g_TeamInfo[MAX_TEAMS+1];
extern int					g_IsSpectator[MAX_PLAYERS+1];


//
//-----------------------------------------------------
//
class CHudDeathNotice : public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_DeathMsg( const char *pszName, int iSize, void *pbuf );

private:
	int m_HUD_d_skull;  // sprite index of skull icon
};

//
//-----------------------------------------------------
//
class CHudMenu : public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	void Reset( void );
	int Draw( float flTime );
	int MsgFunc_ShowMenu( const char *pszName, int iSize, void *pbuf );

	void SelectMenuItem( int menu_item );

	int m_fMenuDisplayed;
	int m_bitsValidSlots;
	float m_flShutoffTime;
	int m_fWaitingForMore;
};

//
//-----------------------------------------------------
//
class CHudSayText : public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_SayText( const char *pszName, int iSize, void *pbuf );
	void SayTextPrint( const char *pszBuf, int iBufSize, int clientIndex = -1 );
	void EnsureTextFitsInOneLineAndWrapIfHaveTo( int line );
	bool IsDead( const char *pszBuf );	// SBO
friend class CHudSpectator;
friend void SboHud_InitChat();	// chat-vgui2: points these at the VGUI2 chat's cvars

private:

	struct cvar_s *	m_HUD_saytext;
	struct cvar_s *	m_HUD_saytext_time;
	struct cvar_s *	m_con_color;	// halflife-updated (halflife #3240): engine con_color for chat text
};

//
//-----------------------------------------------------
//
class CHudBattery: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	int MsgFunc_Battery(const char *pszName,  int iSize, void *pbuf );
	
private:
	HSPRITE m_hSprite1;
	HSPRITE m_hSprite2;
	wrect_t *m_prc1;
	wrect_t *m_prc2;
	int	  m_iBat;	
	float m_fFade;
	int	  m_iHeight;		// width of the battery innards
};


//
//-----------------------------------------------------
//
class CHudFlashlight: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	void Reset( void );
	int MsgFunc_Flashlight(const char *pszName,  int iSize, void *pbuf );
	int MsgFunc_FlashBat(const char *pszName,  int iSize, void *pbuf );
	
private:
	HSPRITE m_hSprite1;
	HSPRITE m_hSprite2;
	HSPRITE m_hBeam;
	wrect_t *m_prc1;
	wrect_t *m_prc2;
	wrect_t *m_prcBeam;
	float m_flBat;	
	int	  m_iBat;	
	int	  m_fOn;
	float m_fFade;
	int	  m_iWidth;		// width of the battery innards
};

//
//-----------------------------------------------------
//
const int maxHUDMessages = 16;
struct message_parms_t
{
	client_textmessage_t	*pMessage;
	float	time;
	int x, y;
	int	totalWidth, totalHeight;
	int width;
	int lines;
	int lineLength;
	int length;
	int r, g, b;
	int text;
	int fadeBlend;
	float charTime;
	float fadeTime;
};

//
//-----------------------------------------------------
//

class CHudTextMessage: public CHudBase
{
public:
	int Init( void );
	static char *LocaliseTextString( const char *msg, char *dst_buffer, int buffer_size );
	static char *BufferedLocaliseTextString( const char *msg );
	char *LookupString( const char *msg_name, int *msg_dest = NULL );
	int MsgFunc_TextMsg(const char *pszName, int iSize, void *pbuf);
};

//
//-----------------------------------------------------
//

class CHudMessage: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	int MsgFunc_HudText(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_GameTitle(const char *pszName, int iSize, void *pbuf);

	float FadeBlend( float fadein, float fadeout, float hold, float localTime );
	int	XPosition( float x, int width, int lineWidth );
	int YPosition( float y, int height );

	void MessageAdd( const char *pName, float time );
	void MessageAdd(client_textmessage_t * newMessage );
	void MessageDrawScan( client_textmessage_t *pMessage, float time );
	void MessageScanStart( void );
	void MessageScanNextChar( void );
	void Reset( void );

private:
	client_textmessage_t		*m_pMessages[maxHUDMessages];
	float						m_startTime[maxHUDMessages];
	message_parms_t				m_parms;
	float						m_gameTitleTime;
	client_textmessage_t		*m_pGameTitle;

	int m_HUD_title_life;
	int m_HUD_title_half;
};

//
//-----------------------------------------------------
//
#define MAX_SPRITE_NAME_LENGTH	24
#define HUD_EXTRA_SPRITES		32	// SBO 2.0: named sprites after hud.txt's: the killfeed icons plugins set (killfeed_icons.cpp)

class CHudStatusIcons: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw(float flTime);
	int MsgFunc_StatusIcon(const char *pszName, int iSize, void *pbuf);

	enum { 
		MAX_ICONSPRITENAME_LENGTH = MAX_SPRITE_NAME_LENGTH,
		MAX_ICONSPRITES = 4,
	};

	
	//had to make these public so CHud could access them (to enable concussion icon)
	//could use a friend declaration instead...
	void EnableIcon( char *pszIconName, unsigned char red, unsigned char green, unsigned char blue );
	void DisableIcon( char *pszIconName );

private:

	typedef struct
	{
		char szSpriteName[MAX_ICONSPRITENAME_LENGTH];
		HSPRITE spr;
		wrect_t rc;
		unsigned char r, g, b;
	} icon_sprite_t;

	icon_sprite_t m_IconList[MAX_ICONSPRITES];

};

//
//-----------------------------------------------------
//
// SBO: sniper scope overlay (code in P3's range, ~0x1002b3a0-0x1002c860).
// SBO 2.0: the sprites are loaded by name (not sprites/hud.txt), one family per renderer: see sniper.cpp
class CHudSniper: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	bool UseScaled( void );		// SBO 2.0: hardware renderer: the scope quadrant, stretched (DrawScaled)
	void LoadQuads( void );
	int DrawScaled( void );

	HSPRITE	m_hCenter;			// 1.38's sprites (software renderer)
	HSPRITE	m_hTopLeft;
	HSPRITE	m_hVBlack;
	HSPRITE	m_hHBlack;
	HSPRITE	m_hTop;
	HSPRITE	m_hTopRight;
	HSPRITE	m_hLeft;
	HSPRITE	m_hRight;
	HSPRITE	m_hBottomLeft;
	HSPRITE	m_hBottom;
	HSPRITE	m_hBottomRight;
	wrect_t	*m_prcCenter;		// their rects (m_rcClassic)
	wrect_t	*m_prcTopLeft;
	wrect_t	*m_prcVBlack;
	wrect_t	*m_prcHBlack;
	wrect_t	*m_prcTop;
	wrect_t	*m_prcTopRight;
	wrect_t	*m_prcLeft;
	wrect_t	*m_prcRight;
	wrect_t	*m_prcBottomRight;
	wrect_t	*m_prcBottom;
	wrect_t	*m_prcBottomLeft;
	int		m_iBorderX;
	int		m_iBorderY;
	bool	m_bDrawn;

	wrect_t	m_rcClassic[11];
	bool	m_bClassic;			// SBO 2.0: every 1.38 sprite this screen needs is loaded

	HSPRITE	m_hQuad[2][2];		// SBO 2.0: the quadrant's tiles (hardware renderer)
	int		m_iQuadTiles;		// tiles per side: 2 (scope_quad_<row><col>), 1 (scope_quad_small), 0 = not loaded
};

#include "particules.h"

// SBO: flame emitters (hud_flamme.cpp). Size 0x5c.
class CHudFlamme: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	~CHudFlamme();
	int MsgFunc_FlammePos( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_PlayerSpawn( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_StateFlamme( const char *pszName, int iSize, void *pbuf );
	Vector GetPosition( void );

	CParticulesEmetteur	*m_pEmetteur;	// 0x14
	CListeEmetteur	m_List;				// 0x18
	int		m_iUnknown20;				// 0x20
	int		m_iUnknown24;				// 0x24
	bool	m_bActive;					// 0x28
	Vector	m_vecPos;					// 0x2c
	int		m_iNumParticules;			// 0x38
	int		m_iSprite;					// 0x3c
	int		m_iSpread;					// 0x40
	int		m_iHeight;					// 0x44
	int		m_iLife;					// 0x48
	float	m_flR;						// 0x4c
	float	m_flG;						// 0x50
	float	m_flB;						// 0x54
	float	m_flA;						// 0x58
};

// SBO: smoke emitters (hud_fumee.cpp). Size 0x74.
class CHudFumee: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	~CHudFumee();
	int MsgFunc_FumeePos( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_StateFumee( const char *pszName, int iSize, void *pbuf );
	Vector GetPosition( void );

	CSmokeEmit	*m_pEmit;				// 0x14
	CListeFumee	m_List;				// 0x18
	int		m_iUnknown20;				// 0x20
	int		m_iUnknown24;				// 0x24
	bool	m_bActive;					// 0x28
	Vector	m_vecPos;					// 0x2c
	int		m_iNumParticules;			// 0x38
	int		m_iSprite;					// 0x3c
	int		m_iParam8;					// 0x40
	int		m_iHeight;					// 0x44
	int		m_iLife;					// 0x48
	int		m_iParam12;					// 0x4c
	int		m_iParam15;					// 0x50
	int		m_iParam16;					// 0x54
	int		m_iParam9;					// 0x58
	int		m_iParam10;					// 0x5c
	int		m_iParam11;					// 0x60
	int		m_iId;						// 0x64
	int		m_iParam13;					// 0x68
	int		m_iParam14;					// 0x6c
	float	m_flParam17;				// 0x70
};

// cs16-client hud.h: env_fog parameters (Fog message, hud_msg.cpp; drawn by RenderFog, tri.cpp)
struct FogParameters
{
	float color[3];
	float density;
	bool affectsSkyBox;
};

extern FogParameters g_FogParameters;

// SBO 2.0 (tri.cpp): a changed fog fades in from the one on screen (env_fogzone edges)
void Fog_Reset( void );							// InitHUD: no fog
void Fog_Snap( void );							// ResetHUD / InitHUD: the fog sent with them applies at once
void Fog_Set( const FogParameters &fog );		// Fog message

// SoHL (LRC, cl_dll/hud.h): env_sky, the 3D sky (CHud::m_iSkyMode)
#define SKY_OFF	0
#define SKY_ON	1

// PROTOTYPE (tri.cpp, docs/WEATHER.md "env_fogzone prototype"): the env_fogzone list (FogZone message)
void FogZone_Clear( void );
void FogZone_VidInit( void );	// the haze sprite, loaded again on every VidInit (CHud::VidInit)
void FogZone_Set( int iIndex, bool bActive, const float *mins, const float *maxs, const float *color, float flDensity );

//
//-----------------------------------------------------
//



class CHud
{
private:
	HUDLIST						*m_pHudList;
	HSPRITE						m_hsprLogo;
	int							m_iLogo;
	client_sprite_t				*m_pSpriteList;
	int							m_iSpriteCount;
	int							m_iSpriteCountAllRes;
	int							m_iSpriteCountHud;	// SBO 2.0: hud.txt's own entries (the extra sprites follow)
	float						m_flMouseSensitivity;
	int							m_iConcussionEffect; 

public:

	HSPRITE						m_hsprCursor;
	float m_flTime;	   // the current client time
	float m_fOldTime;  // the time at which the HUD was last redrawn
	double m_flTimeDelta; // the difference between flTime and fOldTime
	Vector	m_vecOrigin;
	Vector	m_vecAngles;
	int		m_iKeyBits;
	int		m_iHideHUDDisplay;
	int		m_iFOV;
	// SBO 2.0: the FOV the engine draws the frame with (HUD_UpdateClientData). It is m_iFOV, except for a spectator
	// over a map overview: m_iFOV is then the spectator's own (the map is the main view), and the frame is drawn
	// with the watched player's FOV while the first-person inset shows that player (CHud::Think). The overview
	// keeps its own view at any frame FOV (V_GetMapFreePosition, V_GetMapChasePosition: OverviewFovScale)
	int		m_iRenderFOV;
	int		OwnFOV( void );	// the FOV m_iFOV has without a zoom: default_fov, 90 when it is 0
	int		m_iSuppressedCaps;	// SBO 2.0 (Featureful "Capability"): PLAYER_SUPPRESS_* bits for the movement and weapon prediction
	// SBO 2.0: a weapon zoom is active (scope overlay, zoomed crosshair). m_iFOV is the player's default_fov when not
	// zoomed, so a default_fov below 90 is not a zoom; a zoom is a FOV below both 90 and default_fov
	bool	IsZoomed( void );
	int		m_Teamplay;
	int		m_iRes;
	cvar_t  *m_pCvarStealMouse;
	cvar_t	*m_pCvarDraw;

	int m_iFontHeight;
	int DrawHudNumber(int x, int y, int iFlags, int iNumber, int r, int g, int b );
	int DrawHudString(int x, int y, int iMaxX, char *szString, int r, int g, int b );
	int DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b );
	int DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b );
	int GetNumWidth(int iNumber, int iFlags);
	// halflife-updated (halflife#2149): width of a number reserving at least `width` digits, and right-to-left digit drawing
	int GetHudNumberWidth(int number, int width, int flags);
	int DrawHudNumberReverse(int x, int y, int number, int flags, int r, int g, int b);

private:
	// the memory for these arrays are allocated in the first call to CHud::VidInit(), when the hud.txt and associated sprites are loaded.
	// freed in ~CHud()
	HSPRITE *m_rghSprites;	/*[HUD_SPRITE_COUNT]*/			// the sprites loaded from hud.txt
	wrect_t *m_rgrcRects;	/*[HUD_SPRITE_COUNT]*/
	char *m_rgszSpriteNames; /*[HUD_SPRITE_COUNT][MAX_SPRITE_NAME_LENGTH]*/

	struct cvar_s *default_fov;
public:
	HSPRITE GetSprite( int index ) 
	{
		return (index < 0) ? 0 : m_rghSprites[index];
	}

	wrect_t& GetSpriteRect( int index )
	{
		// SBO 2.0: a sprite that hud.txt does not have (GetSpriteIndex: -1) gets an empty rectangle of its own. The
		// caller may write to it (sniper.cpp VidInit): m_rgrcRects[-1] is the heap block's header.
		if ( index < 0 )
		{
			static wrect_t rcNone;
			rcNone.left = rcNone.right = rcNone.top = rcNone.bottom = 0;
			return rcNone;
		}

		return m_rgrcRects[index];
	}

	
	int GetSpriteIndex( const char *SpriteName );	// gets a sprite index, for use in the m_rghSprites[] array

	// SBO 2.0: up to HUD_EXTRA_SPRITES named sprites after hud.txt's, found by GetSpriteIndex like those (the killfeed
	// icons plugins set, killfeed_icons.cpp). A name hud.txt has keeps hud.txt's sprite (GetSpriteIndex finds it first).
	void ClearExtraSprites( void );
	int AddExtraSprite( const char *name, HSPRITE hSprite, const wrect_t &rc );	// the new index, -1: full / bad

	CHudAmmo		m_Ammo;
	CHudHealth		m_Health;
	CHudSpectator		m_Spectator;
	CHudGeiger		m_Geiger;
	CHudBattery		m_Battery;
	CHudTrain		m_Train;
	CHudFlashlight	m_Flash;
	CHudMessage		m_Message;
	CHudStatusBar   m_StatusBar;
	CHudDeathNotice m_DeathNotice;
	CHudSayText		m_SayText;
	CHudMenu		m_Menu;
	CHudAmmoSecondary	m_AmmoSecondary;
	CHudTextMessage m_TextMessage;
	CHudStatusIcons m_StatusIcons;
	CHudSniper		m_Sniper;		// SBO
	CHudFlamme		m_Flamme;		// SBO
	CHudFumee		m_Fumee;		// SBO

	void Init( void );
	void VidInit( void );
	void Think(void);
	int Redraw( float flTime, int intermission );
	int UpdateClientData( client_data_t *cdata, float time );

	CHud() : m_iSpriteCount(0), m_pHudList(NULL) {}  
	~CHud();			// destructor, frees allocated memory

	// user messages
	int _cdecl MsgFunc_Damage(const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_GameMode(const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_Logo(const char *pszName,  int iSize, void *pbuf);
	int _cdecl MsgFunc_ResetHUD(const char *pszName,  int iSize, void *pbuf);
	void _cdecl MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_Capability( const char *pszName, int iSize, void *pbuf );
	void _cdecl MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_SetFOV(const char *pszName,  int iSize, void *pbuf);
	int  _cdecl MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf );
	int  _cdecl MsgFunc_GlowShell( const char *pszName, int iSize, void *pbuf );	// SBO, unreferenced
	int _cdecl MsgFunc_Fog( const char *pszName, int iSize, void *pbuf );	// cs16-client: env_fog
	int _cdecl MsgFunc_FogZone( const char *pszName, int iSize, void *pbuf );	// PROTOTYPE: env_fogzone list
	int _cdecl MsgFunc_RoundTime( const char *pszName, int iSize, void *pbuf );	// SBO 2.0: spectator timer
	int _cdecl MsgFunc_SetSky( const char *pszName, int iSize, void *pbuf );	// SoHL: env_sky

	// SBO 2.0: spectator timer ("RoundTime": long seconds, byte mode ROUNDTIME_*; dlls/multiplay_gamerules.cpp
	// RoundTime_*). The server sends the time when it changes basis, the client runs the clock (as Counter-Strike's
	// round timer does). Until the first message of a map: the time the map has been running (the client's clock).
	int		m_iRoundTimeMode = 1;	// ROUNDTIME_ELAPSED
	float	m_flRoundTimeBase = 0;	// client time the countdown ends / the count up started at; stopped: the seconds
	void ResetRoundTime( void );
	//! The seconds to show now. pflNextChange: seconds until that changes (0 = it doesn't).
	int GetRoundTime( float *pflNextChange );

	// SoHL (LRC; AJH in 1.8: the scale) env_sky, the 3D sky ("SetSky": byte mode, coord x y z, byte scale; dlls/effects.cpp
	// CEnvSky). view.cpp draws the map's sky room from the sky camera before each view (docs/ENV_SKY.md).
	int		m_iSkyMode = SKY_OFF;
	Vector	m_vecSkyPos;
	int		m_iSkyScale = 0;		// parallax: the camera moves 1/scale of the view's offset from the map origin; 0 none
	bool	m_bSkyDrawPass = false;	// SBO 2.0: the engine is drawing the sky room (no client models or effects in it)

	cvar_t	*cl_weather = nullptr;	// cs16-client: weather intensity 0-3 (environment.cpp)

	// Screen information
	SCREENINFO	m_scrinfo;

	int	m_iWeaponBits;		// weapon ids 0..30 and the suit (31): the client data (clientdata_t weapons)
	// SBO 2.1 (Half-Life Unified SDK, HLU #98): weapon ids 32..63, bit (id - 32), from the server's WeaponsHigh
	// message; cleared by ResetHUD (CHudAmmo::Reset)
	unsigned int m_iWeaponBitsHigh = 0;
	// SBO 2.1: whether he holds the weapon with this id (0..63)
	bool HasWeaponBit( int iId ) const
	{
		if ( iId < 0 || iId >= MAX_WEAPONS )
			return false;
		return iId < 32 ? ( (unsigned int)m_iWeaponBits & ( 1u << iId ) ) != 0 : ( m_iWeaponBitsHigh & ( 1u << ( iId - 32 ) ) ) != 0;
	}
	// SBO 2.1: any weapon (ids 0..63), the suit not counted
	bool HasAnyWeapon( void ) const { return ( (unsigned int)m_iWeaponBits & ~( 1u << WEAPON_SUIT ) ) != 0 || m_iWeaponBitsHigh != 0; }
	int	m_fPlayerDead;
	int m_iIntermission;

	// sprite indexes
	int m_HUD_number_0;


	void AddHudElem(CHudBase *p);

	float GetSensitivity();

	// halflife-updated PR #245 (HUD scaling via triangle API, hud_scale.cpp): HUD elements lay out in
	// GetWidth() x GetHeight() HUD units; with hud_scale that is the screen divided by HUD_GetScale(), else the screen
	bool ShouldScale() const { return m_bShouldScale; }
	int GetWidth() const { return m_iWidth; }
	int GetHeight() const { return m_iHeight; }

private:
	cvar_t	*m_pCvarScale = nullptr;
	cvar_t	*m_pCvarClientOS = nullptr;	// SBO 2.0: cl_sbo_os (client_os_shared.h), kept at the build's OS
	bool	m_bShouldScale = false;
	int		m_iWidth = 640;
	int		m_iHeight = 480;

	void UpdateScalingInfo();
};

// hud_scale.cpp: the crosshair while scaling (the engine's is drawn unscaled)
struct ref_params_s;
void HUD_SetRefParams( const struct ref_params_s *pparams );	// main view of V_CalcRefdef
void CL_DrawCrosshair( int intermission );					// HUD_Redraw, before the HUD

// "RoundTime" modes, must match the server (dlls/player.h)
#define ROUNDTIME_COUNTDOWN		0	// seconds left: counts down to 0 and stays there
#define ROUNDTIME_ELAPSED		1	// seconds elapsed: counts up
#define ROUNDTIME_STOPPED		2	// shows the seconds, doesn't run

class TeamFortressViewport;

extern CHud gHUD;
extern TeamFortressViewport *gViewPort;

// SBO 2.0: AFK sign above players (entity.cpp; server: mp_afk_time)
void PlayerAfk_Init( void );
void PlayerAfk_VidInit( void );
void PlayerAfk_Reset( void );

// SBO 2.0: material system, the map's materials file (materials.cpp; docs/MATERIALS.md)
void Materials_Init( void );
void Materials_VidInit( void );
struct pmtrace_s;
struct mat_fx_s;
struct model_s;
char Materials_TraceTexture( struct pmtrace_s *pTrace, char *stripped );
void Materials_Impact( struct pmtrace_s *pTrace, const struct mat_fx_s *fx, const char *hlDecal );
struct model_s *Materials_Sprite( const char *name );	// loaded once per map; NULL if missing
#ifdef SBO_TEST_COMMANDS
bool Materials_TestPrint( void );	// test_cl_materials (test builds only)
void Materials_TestFrame( void );
#endif

// SBO 2.0: "ViewModel" message, the view model's render properties (view.cpp; docs/VIEWMODEL.md)
void V_ResetViewModelRender( void );

extern int g_iPlayerClass;
extern int g_iTeamNumber;
extern int g_iUser1;
extern int g_iUser2;
extern int g_iUser3;

