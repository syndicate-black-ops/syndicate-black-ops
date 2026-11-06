//=========================================================
// SBO: co-operative game mode (co_ maps)
//=========================================================
#ifndef COOP_H
#define COOP_H

class CCoop : public CHalfLifeMultiplay
{
public:
	CCoop();

	virtual void Think( void );
	virtual void InitHUD( CBasePlayer *pl );
	virtual void ClientDisconnected( edict_t *pClient );
	virtual BOOL ClientCommand( CBasePlayer *pPlayer, const char *pcmd );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual BOOL FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled );
	virtual void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual BOOL PlayTextureSounds( void ) { return FALSE; }

	// SBO round control (new virtuals, in vtable order)
	virtual int IsCoop( void );
	virtual void SetupRound( void );
	virtual void CheckWho( void );
	virtual void CheckRound( void );
	virtual void RoundWinner( void );
	virtual void MissionEnd( void );
	virtual void CheckRoundOver( void );

	int m_iRoundState;	// 0x38
	int m_iRoundTime;	// 0x3c
};

#endif // COOP_H
