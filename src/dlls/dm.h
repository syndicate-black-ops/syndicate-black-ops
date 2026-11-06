//=========================================================
// SBO: deathmatch game mode (default)
//=========================================================
#ifndef DM_H
#define DM_H

class CDm : public CHalfLifeMultiplay
{
public:
	CDm();

	virtual void Think( void );
	virtual void InitHUD( CBasePlayer *pl );
	virtual void ClientDisconnected( edict_t *pClient );
	virtual BOOL ClientCommand( CBasePlayer *pPlayer, const char *pcmd );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual BOOL FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled );
	virtual void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual BOOL PlayTextureSounds( void ) { return FALSE; }

	// SBO round control (new virtuals, in vtable order)
	virtual int IsDm( void );
	virtual void SetupRound( void );
	virtual void CheckRound( void );
	virtual void RoundWinner( void );

	int m_iRoundState;		// RoundState (gamerules.h)
	int m_iNextRoundStep;	// was m_iRoundTime: gpGlobals->time (whole seconds) of the next state step

	// SBO 2.0 spectator mode
	void PlayerStartsSpectating( CBasePlayer *pPlayer );
	void PlayerStopsSpectating( CBasePlayer *pPlayer );
	void PlayerLoadoutJoins( CBasePlayer *pPlayer );	// the loadout menu confirmed by a dead or waiting player

	// SBO 2.0: a player with a loadout joins; the first one starts the round as the hunted
	void PlayerJoins( CBasePlayer *pPlayer );
};

#endif // DM_H
