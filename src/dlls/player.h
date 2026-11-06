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
#ifndef PLAYER_H
#define PLAYER_H


#include "pm_materials.h"
#include "client_sendqueue.h"	// SBO 2.0: CBasePlayer::m_SendQueue


#define PLAYER_FATAL_FALL_SPEED		1024// approx 60 feet
#define PLAYER_MAX_SAFE_FALL_SPEED	580// approx 20 feet
#define DAMAGE_FOR_FALL_SPEED		(float) 100 / ( PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED )// damage per unit per second.
#define PLAYER_MIN_BOUNCE_SPEED		200
#define PLAYER_FALL_PUNCH_THRESHHOLD (float)350 // won't punch player's screen/make scrape noise unless player falling at least this fast.

// SBO 2.0, from Featureful: trigger_camera flags the player code reads (triggers.cpp CTriggerCamera)
#define SF_CAMERA_PLAYER_INVULNERABLE		256			// "Player Invulnerable": no damage while the camera has the view
#define SF_CAMERA_STOP_BY_PLAYER_INPUT_USE	(1<<25)		// "Stop by player input" = USE input (key, set at spawn)

//
// Player PHYSICS FLAGS bits
//
#define		PFLAG_ONLADDER		( 1<<0 )
#define		PFLAG_ONSWING		( 1<<0 )
#define		PFLAG_ONTRAIN		( 1<<1 )
#define		PFLAG_ONBARNACLE	( 1<<2 )
#define		PFLAG_DUCKING		( 1<<3 )		// In the process of ducking, but totally squatted yet
#define		PFLAG_USING			( 1<<4 )		// Using a continuous entity
#define		PFLAG_OBSERVER		( 1<<5 )		// player is locked in stationary cam mode. Spectators can move, observers can't.

//
// generic player
//
//-----------------------------------------------------
//This is Half-Life player entity
//-----------------------------------------------------
#define CSUITPLAYLIST	4		// max of 4 suit sentences queued up at any time

#define SUIT_GROUP			TRUE
#define	SUIT_SENTENCE		FALSE

#define	SUIT_REPEAT_OK		0
#define SUIT_NEXT_IN_30SEC	30
#define SUIT_NEXT_IN_1MIN	60
#define SUIT_NEXT_IN_5MIN	300
#define SUIT_NEXT_IN_10MIN	600
#define SUIT_NEXT_IN_30MIN	1800
#define SUIT_NEXT_IN_1HOUR	3600

#define CSUITNOREPEAT		32

#define	SOUND_FLASHLIGHT_ON		"items/flashlight1.wav"
#define	SOUND_FLASHLIGHT_OFF	"items/flashlight1.wav"

#define TEAM_NAME_LENGTH	16

typedef enum
{
	PLAYER_IDLE,
	PLAYER_WALK,
	PLAYER_JUMP,
	PLAYER_SUPERJUMP,
	PLAYER_DIE,
	PLAYER_ATTACK1,
} PLAYER_ANIM;

#define MAX_ID_RANGE 2048
#define SBAR_STRING_SIZE 128

enum sbar_data
{
	SBAR_ID_TARGETNAME = 1,
	SBAR_ID_TARGETHEALTH,
	SBAR_ID_TARGETARMOR,
	SBAR_END,
};

// chat flood protection (chat-vgui2 branch, from BugfixedHL-Rebased): a message is allowed every CHAT_INTERVAL
// seconds; up to CHAT_FLOOD faster messages are tolerated (e.g. the client splitting long input into several
// say commands), after that the sender is muted for CHAT_INTERVAL + CHAT_PENALTY seconds.
#define CHAT_FLOOD          3
#define CHAT_INTERVAL       0.5f
#define CHAT_PENALTY        2.0f

// m_iTextMenu: none of the game's text menus is on screen; a "menuselect" then answers a plugin's menu
#define TEXTMENU_NONE	-1
#define TEXTMENU_GM		4	// SBO 2.0 GM mode's text menus (gmmode.cpp GmShowMenu: quick menu, Misc., orders, classic lists)

class CBasePlayer : public CBaseMonster
{
public:
	// SBO: player fields before random_seed (CBaseMonster ends at m_pCine; CZombie and CDeadHEV are
	// sizeof(CBaseMonster)+4 = 0x2a0 on the server). Names are invented (unknown originals). The comments give each
	// field's 1.38 offset: "0xXXX" or "was m_iSboXXX" (its decompile name), server offset XXX (client = server - 0x14).
	int					m_iCloak;				// 0x29c: cloak active (CBasePlayer::Cloak toggles it)
	int					m_iCybereye;			// 0x2a0: cybereye active (CBasePlayer::Cybereye toggles it)
	BOOL				m_bOutOfPlay;			// 0x2a4 (was m_iSpectator): 1 = not in play: dead or observing (CheckObserver; 1.38 Host_Say used it for "*DEAD*"). Not the spectator mode (m_bSpectating)
	BOOL				m_bNotInGame;			// was m_iSbo2a8: 1 = the rounds don't count this player: disconnected, or Game Master (0 from InitHUD on)
	int					m_iLoadoutModel;		// was m_iSbo2ac: loadout agent model ("malefemale"): 1 = female (agentf), 2 = male (agent), 0 = none picked (agent); co-op teamkiller ban: 0
	int					m_iLoadoutBlade;		// was m_iSbo2b0: loadout blade ("blade"): 1 = warhammer, 2 = chainsaw, 3 = sword, 0 = none
	int					m_iLoadoutLight;		// was m_iSbo2b4: loadout light weapon ("light"): 1 = RC 28, 2 = .444k, 3 = canon, 4 = PM 66, 0 = none (Assassin clears it)
	int					m_iLoadoutHeavy;		// was m_iSbo2b8: loadout heavy weapon, 1-9 (the "heavy" command; 7 = grenade launcher, 9 = nuclear launcher), 0 = none
	int					m_iLoadoutArmor;		// was m_iSbo2bc: loadout armor ("armor"): 1 = armor skin, 2 = torso, 3 = torso and skull, 0 = none ("armor 4"); the spawn sets m_iArmorType from it
	int					m_iLoadoutCyberware;	// was m_iSbo2c0: loadout cyberware ("cyberware"): 1 = cyber legs, 2 = targeting system, 3 = medikit, 4 = adrenaline surge, 5 = Assassin, 6 = cyber eyes, 0 = none
	int					m_iLoadoutItem;			// was m_iSbo2c4: loadout equipment ("item", the menu's last step): 1 = grenades, 2 = tripmines, 3 = satchels, 4 = snarks, 0 = none (Assassin clears it)
	int					m_iBrModelRoll;			// was m_iSbo2c8: BR spawn: the model roll, 1 = female (agentf), 2 = male (agent)
	BOOL				m_bMaleAgent;			// was m_iSbo2cc: 1 = male agent model, the co-op radio plays the male voice; 0 = female (agentf), the radio's "f" sounds (set by co-op PlayerSpawn)
	int					m_iBrBonusRoll;			// was m_iSbo2d0: BR spawn: the bonus roll, 1-40 (the item, weapon or armor the BR spawn gives)
	int					m_iArmorType;			// was m_iSbo2d4: armor worn, picks TakeDamage's absorption: 0 = none, 1 = torso, 2 = torso and skull, 3 = armor skin (0 at every spawn)
	int					m_iCanCloak;			// 0x2d8: class has the cloak power
	int					m_iCanCybereye;			// 0x2dc: class has the cybereye power
	BOOL				m_bTargeting;			// was m_iSbo2e0: 1 = targeting system (cyberware 2, or Assassin): the player's hits do double damage (CBasePlayer::TakeDamage, CBaseMonster::TakeDamage)
	int					m_iSboMaxSpeed;			// 0x2e4: CFederal unzoom restores pfnSetClientMaxspeed from it
	BOOL				m_bLoadoutReady;		// was m_iSbo2e8: 1 = loadout complete (co-op, bounty: the menu's last step, "item"; BR: pressed a key in the go menu)
	int					m_iSpawnMode;			// was m_iSbo2ec: how the modes' PlayerSpawn spawns the player: 0 = waiting, never in play on this map, 1 = as an observer, 2 = alive, in play
	int					m_iSpawnProtectTime;	// was m_iSbo2f4: spawn protection: no damage while gpGlobals->time is below it (spawn + 2 s, Bounty Killer + 1 s; an int, the fraction cut)
	int					m_iCloakSoundTime;		// 0x2f8: next time the cloak sound/effect may play
	int					m_iTeamKills;			// was m_iSbo2fc: co-op player kills this map (0 at InitHUD); 5 or more with mp_ban 1: spawned with no weapons, 3 health and the hunted model ("you are a teamKiller")
	BOOL				m_bHunted;				// was m_iSbo300: 1 = the hunted (Bounty Killer)
	BOOL				m_bGameMaster;			// was m_iSbo304: 1 = Game Master (co-op "gmon")
	int					m_iXP;					// was m_iSbo308: experience (SBO_XP): damage to monsters adds, level up at 5000, halved on a co-op death, 0 on joining
	int					m_iSboSpeed;			// 0x30c: the co-op level, 300 + 5 per level (300 by default: CHalfLifeMultiplay::InitHUD); no lower limit (SboLevelSpeed)
	int					m_iSboHealth;			// 0x310: spawn health, 100 by default (InitHUD, Spawn)
	int					m_iSboDeployFlag;		// 0x314: SBO weapons' Deploy() sets 0 (guns) or 1 (CWong)
	int					m_iTextMenu;			// was m_iSbo318: the game's ShowMenu text menu on screen, which the next "menuselect" answers: TEXTMENU_NONE, 0 = radio, 1 = BR go menu (#Br), (2 / 3 were the old Game Master menus; GM mode uses its VGUI2 spawn window now)
	int					random_seed;    // See that is shared between client & server for shared weapons code

	int					m_iPlayerSound;// the index of the sound list slot reserved for this player
	int					m_iTargetVolume;// ideal sound volume. 
	int					m_iWeaponVolume;// how loud the player's weapon is right now.
	int					m_iExtraSoundTypes;// additional classification for this weapon's sound
	int					m_iWeaponFlash;// brightness of the weapon flash
	float				m_flStopExtraSoundTime;
	
	float				m_flFlashLightTime;	// Time until next battery draw/Recharge
	int					m_iFlashBattery;		// Flashlight Battery Draw

	int					m_afButtonLast;
	int					m_afButtonPressed;
	int					m_afButtonReleased;
	
	// halflife-updated (halflife #1144/#2936, HLU #164): room type is tracked per player and sent from UpdateClientData
	EHANDLE				m_SndLast;				// last sound entity to modify player room type
	int					m_SndRoomtype;			// last roomtype set by sound entity. 0 on a new map (DSP off)
	int					m_ClientSndRoomtype;	// room type last sent to the client, -1 forces a resend
	float				m_flSndRange;			// dist from player to sound entity

	float				m_flFallVelocity;
	
	int					m_rgItems[MAX_ITEMS];
	int					m_fKnownItem;		// True when a new item needs to be added
	int					m_fNewAmmo;			// True when a new item has been added

	unsigned int		m_afPhysicsFlags;	// physics flags - set when 'normal' physics should be revisited or overriden
	float				m_fNextSuicideTime; // the time after which the player can next use the suicide command


// these are time-sensitive things that we keep track of
	float				m_flTimeStepSound;	// when the last stepping sound was made
	float				m_flTimeWeaponIdle; // when to play another weapon idle animation.
	float				m_flSwimTime;		// how long player has been underwater
	float				m_flDuckTime;		// how long we've been ducking
	float				m_flWallJumpTime;	// how long until next walljump

	float				m_flSuitUpdate;					// when to play next suit update
	int					m_rgSuitPlayList[CSUITPLAYLIST];// next sentencenum to play for suit update
	int					m_iSuitPlayNext;				// next sentence slot for queue storage;
	int					m_rgiSuitNoRepeat[CSUITNOREPEAT];		// suit sentence no repeat list
	float				m_rgflSuitNoRepeatTime[CSUITNOREPEAT];	// how long to wait before allowing repeat
	int					m_lastDamageAmount;		// Last damage taken
	float				m_tbdPrev;				// Time-based damage timer

	float				m_flgeigerRange;		// range to nearest radiation source
	float				m_flgeigerDelay;		// delay per update of range msg to client
	int					m_igeigerRangePrev;
	int					m_iStepLeft;			// alternate left/right foot stepping sound
	char				m_szTextureName[CBTEXTURENAMEMAX];	// current texture name we're standing on
	char				m_chTextureType;		// current texture type

	int					m_idrowndmg;			// track drowning damage taken
	int					m_idrownrestored;		// track drowning damage restored

	int					m_bitsHUDDamage;		// Damage bits for the current fame. These get sent to 
												// the hude via the DAMAGE message
	BOOL				m_fInitHUD;				// True when deferred HUD restart msg needs to be sent
	BOOL				m_fGameHUDInitialized;
	int					m_iTrain;				// Train control position
	BOOL				m_fWeapon;				// Set this to FALSE to force a reset of the current weapon HUD info

	EHANDLE				m_pTank;				// the tank which the player is currently controlling,  NULL if no tank
	EHANDLE				m_hViewEntity;			// halflife-updated: the trigger_camera the player looks through, NULL if its own view
	BOOL				m_bTriggerFrozen;		// SBO 2.0: frozen by a trigger_playerfreeze (featureful_triggers.cpp): PreThink keeps him frozen

	float				m_fDeadTime;			// the time at which the player died  (used in PlayerDeathThink())

	BOOL			m_fNoPlayerSound;	// a debugging feature. Player makes no sound if this is true. 
	BOOL			m_fLongJump; // does this player have the longjump module?

	float       m_tSneaking;
	int			m_iUpdateTime;		// stores the number of frame ticks before sending HUD update messages
	int			m_iClientHealth;	// the health currently known by the client.  If this changes, send a new
	int			m_iClientBattery;	// the Battery currently known by the client.  If this changes, send a new
	int			m_iHideHUD;		// the players hud weapon info is to be hidden
	int			m_iClientHideHUD;
	int			m_iFOV;			// field of view
	int			m_iClientFOV;	// client's known FOV
	// usable player items 
	CBasePlayerItem	*m_rgpPlayerItems[MAX_ITEM_TYPES];
	CBasePlayerItem *m_pActiveItem;
	CBasePlayerItem *m_pClientActiveItem;  // client version of the active item
	CBasePlayerItem *m_pLastItem;
	// shared ammo slots
	int	m_rgAmmo[MAX_AMMO_SLOTS];
	int	m_rgAmmoLast[MAX_AMMO_SLOTS];

	Vector				m_vecAutoAim;
	BOOL				m_fOnTarget;
	int					m_iDeaths;
	float				m_iRespawnFrames;	// used in PlayerDeathThink() to make sure players can always respawn

	int m_lastx, m_lasty;  // These are the previous update's crosshair angles, DON"T SAVE/RESTORE

	int m_nCustomSprayFrames;// Custom clan logo frames for this player
	float	m_flNextDecalTime;// next time this player can spray a decal

	char m_szTeamName[TEAM_NAME_LENGTH];

	virtual void Spawn( void );
	void Pain( void );

//	virtual void Think( void );

	// SBO: class/ability virtuals (names from the server's CBasePlayer vtable, slots 129-146; SBO 2.0 removed the 15 GM
	// menu creators Imp() .. KillThemAll(), now gmmode.cpp's spawn catalog).
	// The client only has empty inline stubs (COMDATs in hl_weapons.obj).
	virtual void Cloak( void );
	virtual void Cybereye( void );
	virtual void FireTWalls( float flTime );

	virtual void Jump( void );
	virtual void Duck( void );
	virtual void PreThink( void );
	virtual void PostThink( void );
	virtual Vector GetGunPosition( void );
	virtual int TakeHealth( float flHealth, int bitsDamageType );
	virtual void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	virtual void	Killed( entvars_t *pevAttacker, int iGib );
	virtual Vector BodyTarget( const Vector &posSrc ) { return Center( ) + pev->view_ofs * RANDOM_FLOAT( 0.5, 1.1 ); };		// position to shoot at
	virtual void StartSneaking( void ) { m_tSneaking = gpGlobals->time - 1; }
	virtual void StopSneaking( void ) { m_tSneaking = gpGlobals->time + 30; }
	virtual BOOL IsSneaking( void ) { return m_tSneaking <= gpGlobals->time; }
	virtual BOOL IsAlive( void ) { return (pev->deadflag == DEAD_NO) && pev->health > 0; }
	virtual BOOL ShouldFadeOnDeath( void ) { return FALSE; }
	virtual	BOOL IsPlayer( void ) { return TRUE; }			// Spectators should return FALSE for this, they aren't "players" as far as game logic is concerned

	virtual BOOL IsNetClient( void ) { return TRUE; }		// Bots should return FALSE for this, they can't receive NET messages
															// Spectators should return TRUE for this
	virtual const char *TeamID( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	void RenewItems(void);
	void PackDeadPlayerItems( void );
	void RemoveAllItems( BOOL removeSuit );
	BOOL SwitchWeapon( CBasePlayerItem *pWeapon );

	// JOHN:  sends custom messages if player HUD data has changed  (eg health, ammo)
	virtual void UpdateClientData( void );
	
	static	TYPEDESCRIPTION m_playerSaveData[];

	// Player is moved across the transition by other means
	virtual int		ObjectCaps( void ) { return CBaseMonster :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void	Precache( void );
	BOOL			IsOnLadder( void );
	BOOL			FlashlightIsOn( void );
	void			FlashlightTurnOn( void );
	void			FlashlightTurnOff( void );
	
	void UpdatePlayerSound ( void );
	void DeathSound ( void );

	int Classify ( void );
	void SetAnimation( PLAYER_ANIM playerAnim );
	void SetWeaponAnimType( const char *szExtention );
	char m_szAnimExtention[32];

	// custom player functions
	virtual void ImpulseCommands( void );
	void CheatImpulseCommands( int iImpulse );

	void StartDeathCam( void );
	void StartObserver( Vector vecPosition, Vector vecViewAngle );
	void ReleaseControls( void );	// SBO 2.0: let go of a func_tank and of train / vehicle controls (player.cpp)
	void ResetViewEntity( void );
	BOOL IsFrozenByCamera( void );	// SBO 2.0: the view is a trigger_camera with "Freeze Player" (player.cpp)	// SBO 2.0: the view back from a trigger_camera to the player (player.cpp)
	CBaseEntity *CameraWithFlag( int iFlag );	// SBO 2.0: the trigger_camera that has the view, if it has iFlag (player.cpp)
#ifndef CLIENT_DLL	// SBO server (observer.cpp, Ricochet-style observer); the 2003 client has no observer code
	void StopObserver( void );
	void	Observer_FindNextPlayer( bool bReverse );
	void	Observer_HandleButtons();
	void	Observer_SetMode( int iMode );
	void	Observer_CheckTarget();				// spectator modes: halflife SDK 2.3 / ReGameDLL_CS (observer.cpp)
	void	Observer_CheckProperties();
	void	Observer_SendTargetInfo();			// SBO 2.0: "SpecHealth2", the watched player's state for the spectator panel
	CBasePlayer *Observer_IsValidTarget( int iPlayerIndex );	// ReGameDLL_CS
	void	Observer_CheckViewModelRender();		// SBO 2.0: the watched player's "ViewModel" state in first person

	// SBO 2.0: "ViewModel" message (halflife issue #199; player.cpp, docs/VIEWMODEL.md): the view model's render
	// properties. Kept for the player until changed or reset (not on a respawn or a weapon switch), sent again with
	// every HUD (re)initialisation, and to spectators watching the player in first person.
	void	SetViewModelRender( int iRenderMode, int iRenderFx, int r, int g, int b, int iRenderAmt, int iSkin, int iBody );
	void	ResetViewModelRender( void );
	void	SendViewModelRender( CBasePlayer *pFrom );	// pFrom's state to this player's client
#endif	// but its CBasePlayer has the fields (client.dll: m_flAmmoStartCharge/m_flNextAmmoBurn sit 0xc later)
	EHANDLE	m_hObserverTarget;
	float	m_flNextObserverInput;
	int		IsObserver() { return pev->iuser1; };
	void	ResetMaxSpeed( void );	// SBO 2.0 (player.cpp): re-applies m_iSboMaxSpeed to the engine, like a weapon deploy
	void	SboLevelUp( void );		// SBO 2.0 (player.cpp): one level up (monster kill / medikit XP)
	void	SboLevelPenalty( void );	// SBO 2.0 (player.cpp): the co-op teamkill / suicide penalty, five levels down

	// SBO 2.0, from Featureful (docs/FEATUREFUL_GUIDE_ENTITIES.md): player_capabilities, player_speed, player_flicker.
	// All end with the life (Spawn). As in Featureful, the client gets the capabilities in the "Capability" message
	// (UpdateClientData) and the speed as the max speed (ApplySpeedOverride, every PreThink).
	void	SetSuppressedCaps( int bits );			// PLAYER_SUPPRESS_* (pm_shared.h)
	void	SetSpeedOverride( float value, BOOL absolute );	// 0: none; else a fraction of m_iSboMaxSpeed or a speed
	void	ApplySpeedOverride( void );				// pev->maxspeed from the game's own speed and the override
	void	SetFlashlightFlicker( BOOL flicker );
	int		m_iSuppressedCaps;
	int		m_iClientSuppressedCaps;	// the capabilities the client last got (-1: send again)
	float	m_flSpeedOverride;
	BOOL	m_bSpeedOverrideAbsolute;
	float	m_flSpeedBase;				// the max speed the game set (weapon, zoom, level, legs), before the override
	float	m_flSpeedApplied;			// the max speed ApplySpeedOverride last set (-1: none)
	BOOL	m_bFlashlightFlicker;
	BOOL	m_bFlickerDark;				// the flashlight is on, its light off for a flicker (FlashlightIsOn stays TRUE)
	float	m_flNextFlashlightFlick;
	void	ShowMenu( int bitsValidSlots, int nDisplayTime, BOOL fNeedMore, char *pszText );	// SBO (player.cpp)


	void AddPoints( int score, BOOL bAllowNegativeScore );
	void AddPointsToTeam( int score, BOOL bAllowNegativeScore );
	BOOL AddPlayerItem( CBasePlayerItem *pItem );
	BOOL RemovePlayerItem( CBasePlayerItem *pItem );
	void DropPlayerItem ( char *pszItemName );
	BOOL HasPlayerItem( CBasePlayerItem *pCheckItem );
	BOOL HasNamedPlayerItem( const char *pszItemName );
	BOOL HasWeapons( void );// do I have ANY weapons?
	void SelectPrevItem( int iItem );
	void SelectNextItem( int iItem );
	void SelectLastItem(void);
	void EquipWeapon( void );	// halflife-updated: redeploy the active weapon or switch to the next best one
	void SelectItem(const char *pstr);
	void ItemPreFrame( void );
	void ItemPostFrame( void );
	void GiveNamedItem( const char *szName );
	void EnableControl(BOOL fControl);

	int  GiveAmmo( int iAmount, char *szName, int iMax );
	void SendAmmoUpdate(void);

	void WaterMove( void );
	void EXPORT PlayerDeathThink( void );
	void PlayerUse( void );

	void CheckSuitUpdate();
	void SetSuitUpdate(char *name, int fgroup, int iNoRepeat);
	void UpdateGeigerCounter( void );
	void CheckTimeBasedDamage( void );

	BOOL FBecomeProne ( void );
	void BarnacleVictimBitten ( entvars_t *pevBarnacle );
	void BarnacleVictimReleased ( void );
	static int GetAmmoIndex(const char *psz);
	int AmmoInventory( int iAmmoIndex );
	int Illumination( void );

	void ResetAutoaim( void );
	Vector GetAutoaimVector( float flDelta  );
	Vector AutoaimDeflection( Vector &vecSrc, float flDist, float flDelta  );

	void ForceClientDllUpdate( void );  // Forces all client .dll specific data to be resent to client.
	void SendWeatherInfo( void );	// ReGameDLL_CS: ReceiveW (env_rain/func_rain, env_snow/func_snow)
	void SendParticleEmitters( void );	// SBO: PlayerSpawn, FlammePos, FumeePos (Flamme_particules, Fumee_particules)

	// SBO 2.0 env_fogzone (triggers.cpp; XDM environment.cpp): the fog this player's client has
	void SendFog( void );			// Fog: the zone's (m_hFogZone), else the map's env_fog, else none
	void UpdateFogZone( void );		// PostThink: the zone changed, send its fog
	EHANDLE m_hFogZoneTouch;		// zone touched since the last UpdateFogZone (set by CEnvFogZone::Touch)
	float m_flNextFogZoneCheck;		// free observers: next search for the zone at the view position
	EHANDLE m_hFogZone;				// zone whose fog the client was sent last
	BOOL m_bInFogZone;				// the client has a zone's fog (m_hFogZone is NULL too when the zone was removed)

	void DeathMessage( entvars_t *pevKiller );

	void SetCustomDecalFrames( int nFrames );
	int GetCustomDecalFrames( void );

	void TabulateAmmo( void );

	float m_flStartCharge;
	float m_flAmmoStartCharge;
	float m_flPlayAftershock;
	float m_flNextAmmoBurn;// while charging, when to absorb another unit of player's ammo?
	
	//Player ID
	void InitStatusBar( void );
	void UpdateStatusBar( void );
	int m_izSBarState[ SBAR_END ];
	float m_flNextSBarUpdateTime;
	float m_flStatusBarDisappearDelay;
	char m_SbarString0[ SBAR_STRING_SIZE ];
	char m_SbarString1[ SBAR_STRING_SIZE ];
#ifndef CLIENT_DLL	// SBO: not in the client (its CBasePlayer is 8 bytes larger than the SDK's, not 0xc)
	float m_flNextChatTime;
	int m_iChatFlood;
	int m_iSboTotalXP;	// chat-vgui2: cumulative XP this map/connection (scoreboard XP column)

	float m_flSboHitMsgTime[5];	// chat-vgui2: last time each hit message was shown (SboHitMessage; 0 never)

	// spectator modes (observer.cpp), the halflife SDK 2.3 / ReGameDLL_CS CBasePlayer fields
	int m_iObserverWeapon;		// weapon of current tracked target
	int m_iObserverLastMode;	// last used observer mode
	BOOL m_bWasFollowing;		// ReGameDLL_CS: lost the target, roaming until a valid one is back
	BOOL has_disconnected;		// ReGameDLL_CS: the edict stays after ClientDisconnect; never a target then

	// SBO 2.0 spectator target info (observer.cpp Observer_SendTargetInfo): what this observer's client was sent last
	int m_iSpecInfoTarget;		// player index; -1: nothing since StartObserver / ForceClientDllUpdate, send again
	int m_iSpecInfoHealth;
	int m_iSpecInfoArmor;
	int m_iSpecInfoWeapon;
	int m_iSpecInfoFlags;
	int m_iSpecInfoBrWins;

	// SBO 2.0 "ViewModel" (SetViewModelRender): the state, in the message's byte order
	BOOL m_bViewModelRender;		// set (FALSE: the view model's own render properties)
	byte m_rgViewModelRender[8];
	int m_iViewModelRenderSerial;	// changes with every SetViewModelRender / ResetViewModelRender
	// what Observer_CheckViewModelRender last sent this spectator: the watched player (0: nothing, the player's own
	// state) and its m_iViewModelRenderSerial then (-1: send again)
	int m_iViewModelRenderFrom;
	int m_iViewModelRenderFromSerial;

	// SBO 2.0: BR rounds won on this map by this player (br.cpp BrLogRoundWinner, the count of its log line)
	int m_iBrRoundWins;

	// halflife-updated: the client's cl_autowepswitch (userinfo), used by CHalfLifeMultiplay::FShouldSwitchWeapon:
	// 0 = never switch to a picked up weapon, 1 = always (SBO's behaviour, the default until the userinfo is read),
	// 2 = not while firing
	int m_iAutoWepSwitch = 1;
	BOOL m_fLeftHanded = FALSE;	// SBO 2.0: the client's cl_righthand > 0 (userinfo): weapons in the left hand
	void SetPrefsFromUserinfo( char *infobuffer );

	// SBO 2.0 round bookkeeping (coop.cpp, br.cpp)
	BOOL m_bRoundParticipant;	// spawned by the round in play (CheckWho): counted in its participant/live numbers
	float m_flLoadoutWaitStart;	// mp_loadouttime: when the player could last make progress choosing a loadout
								// (InitHUD, a loadout menu step); 0 = not set yet
	BOOL m_bMissionEndObserver;	// co-op: alive when the held mission end made the living observers
								// (CCoop::MissionEndObservers): gets the radio back after voting (CCoop::CanUseRadio)
	BOOL m_bMissionEndVoted;	// co-op: has answered a menu that isn't the game's since then, i.e. the map vote's
								// ("menuselect", CCoop::ClientCommand)

	// SBO 2.0 spectator mode (observer.cpp; toggled by the "spectate" command or /spec in chat, client.cpp): out of
	// the rounds until the player toggles it off; kept across changelevel and reconnects (client.cpp)
	BOOL m_bSpectating;
	float m_flNextSpectateToggle;	// the mode can't be toggled again before this time (anti-spam)
	float m_flNextFullUpdate;		// "fullupdate" (client.cpp) is ignored before this time
	BOOL m_bSpectateIdPending;		// identity not validated by Steam yet: asked again every frame (client.cpp)
	BOOL m_bSpectateMenuExit;		// confirmed the selection menu (Go !, "item"): leaves the mode this frame (client.cpp)
	float m_flRoundJoinTime;		// co-op: left the mode with nobody alive in a round: no round candidate before this
									// time (CCoop::PlayerStopsSpectating); 0 = none
	void StartSpectating( void );
	void StopSpectating( void );

	// SBO 2.0: a dead or waiting player confirmed the loadout menu ("item", CCoop / CDm::ClientCommand): joins once
	// all the menu's commands are in (UpdateClientData, LoadoutJoin), so the spawn has the new cyberware
	BOOL m_bLoadoutJoinPending;
	void LoadoutJoin( void );

	// SBO 2.0 (client_sendqueue.h): map state sent to this client a few messages at a time after its HUD was
	// initialised (env_particle systems, env_static props, point_worldtext texts), one state per sender
	sendqueue_state_t m_SendQueue[ SENDQUEUE_MAX ];

	// SBO 2.0 head scale ("headscale" key, head_scale.cpp, docs/MODEL_SCALE.md "Head scale"): the head drawn and hit
	// this many times its size (0: never set, 1)
	float m_flHeadScale;
	void SetHeadScale( float flScale );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void SetObjectCollisionBox( void );

	// SBO 2.0 Game Master mode plugin hooks (gmmode.cpp, docs/GM_MODE.md "Plugin hooks"; pick: game_shared/
	// gm_catalog.h GM_PICK). Plugins hook them with Ham Sandwich under the keys sc_player_canstartnextvote and
	// sc_item_collect (AMX Mod X gamedata keys), whose prototypes these match: bool (int) and void (CBaseEntity *,
	// int). The game itself allows everything and does nothing after a placement.
	// May this Game Master place this pick now? Asked on the pick in the spawn window and on each placing click.
	virtual bool GmCanPlace( int iPick ) { return true; }
	// This Game Master placed pEntity (a monster, a gm_weapon_drop, a gm_teleporter_pad), spawned and owned.
	virtual void GmPlaced( CBaseEntity *pEntity, int iPick ) {}

	// SBO 2.0 AFK players (player.cpp; mp_afk_time, mp_afk_spectate_time; m_fLastMovement in ReGameDLL_CS, which
	// only kicks): the time a player in play has given no input. It only runs while the player is in play, and only
	// input resets it, so it carries over deaths and rounds.
	float m_flAfkTime;			// seconds in play without input
	float m_flAfkLastThink;		// gpGlobals->time of the last AfkThink (0 = none yet): the time is counted from the clock
	float m_flAfkNextLog;		// developer 2: the next "AFK: ... active" line for a player without the sign (one a second)
	BOOL m_bAfkWasInPlay;		// AfkInPlay at the last AfkThink

	BOOL m_bSafeFall;			// SBO 2.0 trigger_safe_fall (Featureful): this fall's landing does no damage
	float m_flAfkIgnoreInput;	// input before this time is no activity (spawn, teleport: the server turned the view)
	int m_bAfkInput;			// a usercmd since the last AfkThink differed from the one before (CmdStart): AFK_INPUT_ bits, what differed
	BOOL m_bAfkMarked;			// the AFK sign is shown above the player (last "PlayerAfk" sent)
	int m_iAfkButtons;			// the last usercmd: buttons, move commands, view angles
	Vector m_vecAfkMove;
	Vector m_vecAfkAngles;
	BOOL AfkInPlay( void );
	BOOL AfkAtControls( void );	// driving a func_vehicle or using a func_tank: a held key is input
	void AfkCheckInput( const struct usercmd_s *cmd );
	void AfkThink( void );
	void AfkActivity( int iInput = 0, const char *pszCommand = NULL );
	void AfkSetMarked( BOOL bMarked );
	void AfkIgnoreInput( void );

	// SBO 2.0 seamless trigger_teleport (triggers.cpp): a landmark teleport turned the player by pev->vuser1.x
	// degrees from the yaw pev->vuser1.y. The moves the client sent before it got the turn are turned too
	// (PM_TeleportStaleCmd, on the client as well); pev->vuser1 is cleared at the first move with the new yaw.
	void TeleportTurned( float flTurn );
	void TeleportCheckCmd( const struct usercmd_s *cmd );	// CmdStart
	void TeleportPostThink( void );
	float m_flTeleportTurnUntil;	// pev->vuser1 is cleared at this time at the latest
	BOOL m_bTeleportStaleCmd;		// this move was sent before the client got the turn
	float m_flTeleportCmdYaw;		// its yaw, turned
	BOOL m_bTeleportAddAngle;		// pev->avelocity.y holds the turn for the engine's svc_addangle (fixangle 2)

	// SBO 2.0 impulse 212 (cheat, player.cpp CheatImpulseCommands / FastMoveUpdate): the player's max speed is held
	// at FASTMOVE_MAXSPEED. The game's own speed code keeps writing pev->maxspeed as before (weapon deploy, zoom,
	// leg hit, level-up); each such write is taken as the normal speed and replaced by the fast one, so turning it
	// off gives back the speed the game has set last.
	void FastMoveUpdate( void );	// PreThink and UpdateClientData
	void FastMoveOff( void );
	BOOL m_bFastMove;
	float m_flFastMoveNormalSpeed;	// the speed without impulse 212 (the last one the game set)

	float m_flNextCyberLight;	// SBO 2.0 cybereye: the next lights ahead of the view (PostThink, CYBEREYE_LIGHT_INTERVAL)

	// SBO 2.0 cybereye: the client keeps one screen fade, so another fade (env_fade, gauss overcharge, ...) replaces
	// the cybereye's stay-out tint. UTIL_ScreenFadeWrite reports each fade; PostThink sends the tint again once that
	// fade has ended (0: no tint to restore).
	void CybereyeTint( void );	// player.cpp: sends the cybereye's stay-out tint
	void ScreenFadeSent( float flTime, int flags );	// util.cpp UTIL_ScreenFadeWrite: the fade's duration + hold
	float m_flCyberTintRestore;
#endif
};

#define AUTOAIM_2DEGREES  0.0348994967025
#define AUTOAIM_5DEGREES  0.08715574274766
#define AUTOAIM_8DEGREES  0.1391731009601
#define AUTOAIM_10DEGREES 0.1736481776669


extern int	gmsgHudText;
extern BOOL gInitHUD;

// SBO 2.0: env_mirror (env_mirror.cpp): the active mirrors to a player whose HUD is (re)initialised
void Mirror_SendAll( CBasePlayer *pPlayer );

// chat-vgui2 branch: SBO scoreboard categories (multiplay_gamerules.cpp; port of the 1.38 scoreboard plugin)
#define SBO_SB_FORCE_TEAMINFO	1
#define SBO_SB_FORCE_SCOREINFO	2
BOOL SboScoreboard_Active( void );
void SboScoreboard_Reset( void );
void SboScoreboard_ClientPutInServer( CBasePlayer *pPlayer );
BOOL SboScoreboard_InPlay( CBasePlayer *pPlayer );	// alive and in the game now: not dead, waiting, in the menus or observing
void SboScoreboard_ClientInit( CBasePlayer *pPlayer );
void SboScoreboard_ClientResend( CBasePlayer *pPlayer );	// "fullupdate": the scoreboard as last sent, to this client only
void SboScoreboard_ClientDisconnected( edict_t *pEntity );
void SboScoreboard_Update( CBasePlayer *pPlayer, int iForce = 0 );
void SboScoreboard_SendScoreInfo( CBasePlayer *pPlayer );
void SboScoreboard_Frame( void );
const char *SboScoreboard_ChatPrefix( CBasePlayer *pPlayer );

// SBO 2.0: scoreboard mode info line (multiplay_gamerules.cpp SboModeInfo_*; "SboModeInfo" message: byte type,
// short value1, short value2). Must match the client (cl_dll/vgui2/sbo_bridge.cpp).
#define SBO_MODEINFO_NONE		0	// no line
#define SBO_MODEINFO_ROUNDS		1	// co-op: value1 current round (0 = first round not started), value2 round limit
#define SBO_MODEINFO_FRAGLIMIT	2	// value1 mp_fraglimit (0 = no frag limit: no line)
void SboModeInfo_Reset( void );
void SboModeInfo_ClientInit( CBasePlayer *pPlayer );
void SboModeInfo_Frame( void );

// SBO 2.0: spectator target info (observer.cpp Observer_SendTargetInfo; "SpecHealth2" message: byte player, short
// health, short armor, byte weapon id, byte flags, byte BR rounds won). Must match the client
// (cl_dll/vgui2/sbo_bridge.cpp).
#define SPECINFO_LEGS_BROKEN	1	// flags: the player's legs are broken
// Broken legs are a speed, as they always were in SBO: a living player whose m_iSboMaxSpeed is below this. 130 is the
// game's own number: a co-op leg hit takes 20 only while the speed is 130 or more (CBasePlayer::TraceAttack), so
// below it the legs can't get worse; the other modes' leg hit (110) and a bad fall (100, PostThink) are below it too.
#define SBO_LEGS_BROKEN_SPEED	130

// SBO 2.0: co-op levels (player.cpp SboLevelSpeed). m_iSboSpeed is the level, 300 + 5 per level, with no lower limit
// (a level-up +5, a teamkill / suicide penalty -25). Its speed effect stops at level -40's speed, 100: a spawn's
// loadout starts from SboLevelSpeed( m_iSboSpeed ), so below -40 the player moves at the level -40 speed and only
// gains speed once the level is above -40 again. SBO_MIN_MAXSPEED is the lowest m_iSboMaxSpeed a loadout or a
// penalty leaves: weapon and armor weights can take a low level's speed to 0 or below, where the server's movement
// stops the player while the client, sent the negative maxspeed as a large unsigned number (delta.lst clientdata
// maxspeed: 16 bits, unsigned), predicts him walking and snaps back to the same spot. 80 is the game's own
// slowest walk (the zoomed sniper rifles: SBO_ZOOM_MAXSPEED).
#define SBO_LEVEL_SPEED_MIN		100
#define SBO_LEVEL_PENALTY_SPEED	25
#define SBO_MIN_MAXSPEED		80
int SboLevelSpeed( int iSboSpeed );

// SBO 2.0: spectator timer (multiplay_gamerules.cpp RoundTime_*; "RoundTime" message: long seconds, byte mode).
// Must match the client (cl_dll/hud.h).
#define ROUNDTIME_COUNTDOWN		0	// seconds left: counts down to 0 and stays there
#define ROUNDTIME_ELAPSED		1	// seconds elapsed: counts up
#define ROUNDTIME_STOPPED		2	// shows the seconds, doesn't run
void RoundTime_Reset( void );
void RoundTime_ClientInit( CBasePlayer *pPlayer );
void RoundTime_Frame( void );

// chat-vgui2 branch: TraceAttack's hit messages ("My fucking head", ...), always throttled per player and message type
// to one per SBO_HITMSG_INTERVAL seconds (player.cpp; native port of sbo_spam_throttle.sma)
#define SBO_HITMSG_INTERVAL	10.0f
#define SBO_HITMSG_HEAD		0
#define SBO_HITMSG_CHEST	1
#define SBO_HITMSG_STOMACH	2
#define SBO_HITMSG_ARM		3
#define SBO_HITMSG_LEG		4
#define SBO_HITMSG_COUNT	5
void SboHitMessage( CBasePlayer *pPlayer, int iType, const char *pszMsg );

// chat-vgui2 branch: kill feed names, native port of the metamod-syndicate-killfeed plugin (multiplay_gamerules.cpp;
// cvar mp_killfeed_npc; "SboDeathMsg" message)
#define SBO_KF_REPLACES_DEATHMSG	1	// SboDeathMsg flag: the stock DeathMsg for this victim follows
void SboKillfeed_PlayerDeath( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pevInflictor, const char *pszWeapon,
	int iHeadshot );
int SboKillfeed_Headshot( CBaseEntity *pVictim );
const char *SboKillfeed_KillerNpcName( entvars_t *pKiller, entvars_t *pevInflictor, char *pszBuf, int iBufLen );
const char *SboKillfeed_InflictorCode( entvars_t *pevInflictor );
void SboKillfeed_NpcKilled( CBaseEntity *pVictim, entvars_t *pevAttacker );

// SBO 2.0 env_particle (env_particle.cpp; Spirit of Half-Life's Aurora particle systems, docs/AURORA.md)
void EnvParticle_ServerActivate( void );			// ServerActivate, before the entities' Activate: their targets
void EnvStatic_Reset( void );						// ServerDeactivate: the map's props are gone

// SBO 2.0 point_worldtext (point_worldtext.cpp, docs/POINT_WORLDTEXT.md)
void WorldText_Reset( void );						// ServerDeactivate: the map's texts are gone

#endif // PLAYER_H
