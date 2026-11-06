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

	int m_iRoundState;	// 0x38
	int m_iRoundTime;	// 0x3c
};

#endif // DM_H
