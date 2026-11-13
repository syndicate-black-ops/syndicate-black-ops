//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SPECTATOR_H
#define SPECTATOR_H
#pragma once

#include "cl_entity.h"
#include "interpolation.h"	// Steam SDK: HLTV camera paths



// SBO 2.0: INSET_CHASE_LOCKED as in Counter-Strike (the inset chases with the target's own view angles, not the
// mouse angles that also turn a full-screen overview); the values after it moved up by one, as there
#define INSET_OFF				0
#define	INSET_CHASE_LOCKED		1
#define	INSET_CHASE_FREE		2
#define	INSET_IN_EYE			3
#define	INSET_MAP_FREE			4
#define	INSET_MAP_CHASE			5

#define MAX_SPEC_HUD_MESSAGES	8



#define OVERVIEW_TILE_SIZE		128		// don't change this
#define OVERVIEW_MAX_LAYERS		8		// XDM hud_spectator.h (the SDK: 1): one image per floor of the map

// SBO 2.0: a floor of a multi-layer overview changes when the height that picks it is this far past the layer's HEIGHT,
// so a player standing at a HEIGHT (stairs, view bob, interpolation) does not flip the picture back and forth
#define OVERVIEW_LAYER_HYSTERESIS	16.0f
// SBO 2.0: the watched player's height in the full-screen overview: the origin plus the standing eye height
// (DEFAULT_VIEWHEIGHT, V_GetInEyePos), which is where the first-person camera is that picks the inset's floor
#define OVERVIEW_WATCHED_VIEWHEIGHT	28.0f

//-----------------------------------------------------------------------------
// Purpose: Handles the drawing of the spectator stuff (camera & top-down map and all the things on it )
//-----------------------------------------------------------------------------

typedef struct overviewInfo_s {
	char		map[64];	// cl.levelname or empty
	vec3_t		origin;		// center of map
	float		zoom;		// zoom of map images
	int			layers;		// how may layers do we have
	float		layersHeights[OVERVIEW_MAX_LAYERS];
	char		layersImages[OVERVIEW_MAX_LAYERS][255];
	qboolean	rotated;	// are map images rotated (90 degrees) ?
	
	int			insetWindowX;
	int			insetWindowY;
	int			insetWindowHeight;
	int			insetWindowWidth;
} overviewInfo_t;

typedef struct overviewEntity_s {

	HSPRITE					hSprite;
	struct cl_entity_s *	entity;
	double					killTime;
} overviewEntity_t;

// Steam SDK (halflife-updated cl_dll/hud_spectator.h): HLTV director camera waypoint (DRC_CMD_WAYPOINTS)
typedef struct cameraWayPoint_s
{
	float		time;
	vec3_t		position;
	vec3_t		angle;
	float		fov;
	int			flags;
} cameraWayPoint_t;

#define	 MAX_OVERVIEW_ENTITIES		128
#define	 MAX_CAM_WAYPOINTS			32

class CHudSpectator : public CHudBase
{
public:
	void Reset();
	int  ToggleInset(bool allowOff);
	int  InsetMode();	// SBO 2.0: the inset shown (spec_pip, or INSET_OFF while its mode has no target to show)
	void CheckSettings();
	void UpdateCrosshair();
	void InitHUDData( void );
	bool AddOverviewEntityToList( HSPRITE sprite, cl_entity_t * ent, double killTime);
	void DeathMessage(int victim);
	bool AddOverviewEntity( int type, struct cl_entity_s *ent, const char *modelname );
	void CheckOverviewEntities();
	void DrawOverview();
	void DrawOverviewEntities();
	float OverviewFovScale();	// SBO 2.0: how much wider the overview's own field of view is than the frame's
	float m_flOverviewNdcScale;	// SBO 2.0: the overview pass's screen positions are scaled by it (OverviewGL_Begin)
	void GetMapPosition( float * returnvec );
	void DrawOverviewLayer();
	void LoadMapSprites();
	void UpdateWatchedHeight();
	bool IsLayerBelowView( int layer );
	bool ParseOverviewFile();
	bool IsActivePlayer(cl_entity_t * ent);
	void SetModes(int iMainMode, int iInsetMode);
	void HandleButtonsDown(int ButtonPressed);
	void HandleButtonsUp(int ButtonPressed);
	void FindNextPlayer( bool bReverse );
	void DirectorMessage( int iSize, void *pbuf );
	void SetSpectatorStartPosition();
	int Init();
	int VidInit();

	int Draw(float flTime);

	// Steam SDK (halflife-updated): HLTV director cameras (DRC_CMD_CAMERA / CAMPATH / WAYPOINTS)
	void	AddWaypoint( float time, vec3_t pos, vec3_t angle, float fov, int flags );
	void	SetCameraView( vec3_t pos, vec3_t angle, float fov );
	float	GetFOV();
	bool	GetDirectorCamera( vec3_t &position, vec3_t &angle );
	void	SetWayInterpolation( cameraWayPoint_t * prev, cameraWayPoint_t * start, cameraWayPoint_t * end, cameraWayPoint_t * next );

	int m_iDrawCycle;

	// SBO 2.0: the view being rendered, set by V_CalcRefdef before each pass (m_iDrawCycle is only valid for spectators).
	// The client's own effects (tri.cpp: flames, smoke, weather, fog) ask these.
	bool m_bOverviewDrawPass;	// XDM hud_spectator.h m_OverviewMapDrawPass: the overview map (full screen or inset), not a camera
	bool m_bInsetDrawPass;		// the spectator's inset window (overview or camera)
	client_textmessage_t m_HUDMessages[MAX_SPEC_HUD_MESSAGES];
	char				m_HUDMessageText[MAX_SPEC_HUD_MESSAGES][128];
	int					m_lastHudMessage;
	overviewInfo_t		m_OverviewData;
	overviewEntity_t	m_OverviewEntities[MAX_OVERVIEW_ENTITIES];
	int					m_iObserverFlags;
	int					m_iSpectatorNumber;
	
	float				m_mapZoom;		// zoom the user currently uses
	vec3_t				m_mapOrigin;	// origin where user rotates around
	cvar_t *			m_drawnames;
	cvar_t *			m_drawcone;
	cvar_t *			m_drawstatus;
	cvar_t *			m_drawinfo;		// SBO 2.0: spec_drawinfo, the watched player's info box (vgui2/sbo_spectator_info.cpp)
	cvar_t *			m_autoDirector;
	cvar_t *			m_pip;
	

	qboolean			m_chatEnabled;

	vec3_t				m_vecMainViewOrigin;	// XDM view.cpp g_vecViewOrigin: the camera of the main view (V_CalcRefdef), picks the layers of the inset overview

	// SBO 2.0: the floor of the full-screen overview follows the watched player (UpdateWatchedHeight)
	bool				m_bWatchedHeightKnown;	// a player was watched on this map
	int					m_iWatchedPlayer;		// the last one
	float				m_flWatchedHeight;		// and the height last seen
	int					m_iLayerViewKey;		// what picked m_bLayerBelowView: 0 the main camera (inset), else the watched player
	bool				m_bLayerBelowView[OVERVIEW_MAX_LAYERS];	// drawn solid (IsLayerBelowView)

	vec3_t				m_cameraOrigin;	// a help camera
	vec3_t				m_cameraAngles;	// and it's angles

	// Steam SDK (halflife-updated): HLTV director camera state
	bool				m_IsInterpolating;	// following a DRC_CMD_WAYPOINTS camera path
	int					m_ChaseEntity;		// if != 0, follow this entity with viewangles (DRC_CMD_CAMERA)
	int					m_WayPoint;			// current waypoint 1
	int					m_NumWayPoints;		// current number of waypoints
	CInterpolation		m_WayInterpolation;

private:
	vec3_t		m_vPlayerPos[MAX_PLAYERS];
	float		m_flPlayerPosTime;	// client time of the frame DrawOverviewEntities filled m_vPlayerPos in
	HSPRITE		m_hsprPlayerBlue;
	HSPRITE		m_hsprPlayerRed;
	HSPRITE		m_hsprPlayer;
	HSPRITE		m_hsprCamera;
	HSPRITE		m_hsprPlayerDead;
	HSPRITE		m_hsprViewcone;
	HSPRITE		m_hsprUnkownMap;
	HSPRITE		m_hsprBeam;
	HSPRITE		m_hCrosshair;

	wrect_t		m_crosshairRect;

	struct model_s * m_MapSprite[OVERVIEW_MAX_LAYERS];	// each layer image is saved in one sprite, where each tile is a sprite frame
	float		m_flNextObserverInput;
	float		m_FOV;			// Steam SDK: director camera FOV (HLTV only, see CHud::Think)
	float		m_zoomDelta;
	float		m_moveDelta;
	int			m_lastPrimaryObject;
	int			m_lastSecondaryObject;

	cameraWayPoint_t	m_CamPath[MAX_CAM_WAYPOINTS];	// Steam SDK: HLTV camera path
};

#endif // SPECTATOR_H
