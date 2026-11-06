//=========================================================
// SBO: battle royale game mode (br_ maps)
//=========================================================
#ifndef BR_H
#define BR_H

class CBr : public CHalfLifeMultiplay
{
public:
	CBr();

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
	virtual int IsBr( void );
	virtual void SetupRound( void );
	virtual void CheckWho( void );
	virtual void CheckRound( void );
	virtual void RoundWinner( void );
	virtual void CheckRoundOver( void );

	int m_iRoundState;	// 0x38
	int m_iRoundTime;	// 0x3c
};

#endif // BR_H
