//=========================================================
// SBO: co-operative game rules (co_ maps)
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"gamerules.h"
#include	"skill.h"
#include	"game.h"
#include	"items.h"
#include	"voice_gamemgr.h"
#include	"coop.h"

extern DLL_GLOBAL CGameRules	*g_pGameRules;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgScoreInfo;
extern CVoiceGameMgr	g_VoiceGameMgr;

extern int m_Changeniv;
extern int m_SpawnLocate;
extern int m_FailMissionGo;
extern int m_Missionpasbon;
extern int m_iMaxRound;
extern int RoundLimit;
extern int Limitat;
extern int m_Rc28, m_K444, m_Canon, m_Pm66, m_Fa18, m_Federal, m_Spa, m_Ben, m_Sw, m_Nm33, m_Mini;

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );
extern void respawn( entvars_t *pev, BOOL fCopyCorpse );
extern void ShowVGUI( CBasePlayer *pPlayer, int iMenu );

int m_iParticip1;		// players taking part in the round
int m_iLive1;			// players still alive
int m_iDeathLimit;		// players alive when the round started
int m_iNumberRound;
int limit;
int m_VoteYes;
int m_VoteNo;
int ReloadMap;
int GmEngage;

//=========================================================
//=========================================================
CCoop::CCoop()
{
	m_Changeniv = 0;
	m_iRoundState = 0;
	m_iParticip1 = 0;
	m_iLive1 = 0;
	m_iDeathLimit = 0;
	m_iRoundTime = -1;
	m_FailMissionGo = 0;
	m_Missionpasbon = 0;
	m_iNumberRound = 0;
	m_iCoopParticip = 0;
	m_iCoopLive = 0;
	m_VoteYes = 0;
	m_VoteNo = 0;
	RoundLimit = 0;
	Limitat = 0;
	ReloadMap = 0;
	GmEngage = 0;
	m_SpawnLocate = 1;
}

//=========================================================
//=========================================================
void CCoop::Think( void )
{
	g_VoiceGameMgr.Update(gpGlobals->frametime);

	if ( m_iRoundState == 0 )
	{
		m_iParticip1 = 0;
		m_iLive1 = 0;
		m_iDeathLimit = 0;

		if ( m_iRoundTime < gpGlobals->time )
			CheckRound();
	}

	if ( m_iRoundState == 4 )
	{
		m_iParticip1 = 0;
		m_iLive1 = 0;
		m_iDeathLimit = 0;
		SetupRound();
	}

	if ( m_iRoundState == 5 )
	{
		m_iParticip1 = 0;
		m_iLive1 = 0;
		m_iDeathLimit = 0;

		if ( m_iRoundTime < gpGlobals->time )
		{
			m_FailMissionGo = 0;
			MissionEnd();
		}
	}

	if ( m_iRoundState == 3 )
	{
		if ( m_FailMissionGo )
		{
			m_iRoundTime = gpGlobals->time + 5;
			m_FailMissionGo = 0;
			m_iRoundState = 5;
		}

		if ( m_iParticip1 < 1 )
		{
			m_iParticip1 = 0;
			m_iLive1 = 0;
			m_iDeathLimit = 0;
			m_iRoundTime = gpGlobals->time + 20;
			m_iRoundState = 0;
			return;
		}

		if ( m_iLive1 <= 0 )
		{
			m_iParticip1 = 0;
			m_iLive1 = 0;
			m_iDeathLimit = 0;
			m_iRoundTime = 0;
			m_iRoundState = 1;
			return;
		}
	}

	if ( m_iRoundState == 2 )
	{
		if ( m_VoteYes + m_VoteNo == m_iParticip1 )
		{
			if ( m_VoteYes > m_VoteNo )
			{
				m_iRoundState = 0;
				m_VoteYes = 0;
				m_VoteNo = 0;
				m_iParticip1 = 0;
				ReloadMap = 1;
				m_FailMissionGo = 0;
				m_Missionpasbon = 0;
				GoToIntermission();
			}
			else
			{
				m_iRoundState = 0;
				m_VoteYes = 0;
				m_VoteNo = 0;
				m_iParticip1 = 0;
				ReloadMap = 0;
				m_FailMissionGo = 0;
				m_Missionpasbon = 0;
				GoToIntermission();
			}
		}
	}

	if ( m_iRoundState == 1 )
	{
		RoundWinner();
		m_iParticip1 = 0;
		m_iDeathLimit = 0;
		m_iLive1 = 0;
	}

	if ( g_fGameOver )   // someone else quit the game already
	{
		CHalfLifeMultiplay::Think();
		return;
	}

	if ( m_Changeniv == 1 )
	{
		m_FailMissionGo = 1;
		m_Missionpasbon = 0;
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "SUCCESSFULL MISSION\n" );
		m_Changeniv = 0;
	}
}

//=========================================================
//=========================================================
int CCoop::IsCoop( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CCoop::InitHUD( CBasePlayer *pPlayer )
{
	pPlayer->m_iSbo2a8 = 0;
	m_Changeniv = 0;

	pPlayer->pev->classname = MAKE_STRING( "observer" );
	pPlayer->pev->solid = SOLID_NOT;
	pPlayer->pev->takedamage = DAMAGE_NO;
	pPlayer->pev->movetype = MOVETYPE_NONE;
	pPlayer->pev->flags |= FL_NOTARGET;
	pPlayer->pev->effects |= EF_NODRAW;
	pPlayer->m_afPhysicsFlags |= PFLAG_OBSERVER;
	pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT | HIDEHUD_HEALTH;
	pPlayer->RemoveAllItems( TRUE );
	pPlayer->m_iSbo2fc = 0;
	pPlayer->m_iSbo2e8 = 0;
	pPlayer->m_iSpectator = 1;

	CHalfLifeMultiplay::InitHUD( pPlayer );
}

//=========================================================
//=========================================================
void CCoop::ClientDisconnected( edict_t *pClient )
{
	if ( pClient )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

		pPlayer->m_iSbo2a8 = 1;
		pPlayer->m_iSbo304 = 0;
		GmEngage = 0;

		if ( m_iRoundState == 3 )
		{
			m_iParticip1--;
			if ( !pPlayer->m_iSpectator )
				m_iLive1--;
			CheckRoundOver();
		}
		else
		{
			m_iParticip1--;
			CheckRound();
		}
	}

	CHalfLifeMultiplay::ClientDisconnected( pClient );
}

//=========================================================
//=========================================================
void CCoop::RoundWinner( void )
{
	if ( easymode.value == 1 )
	{
		if ( RoundLimit == 1 )
		{
			if ( m_iNumberRound >= m_iMaxRound * 1.5 )
			{
				m_FailMissionGo = 1;
				m_Missionpasbon = 1;
				UTIL_ClientPrintAll( HUD_PRINTCENTER, "FAILED MISSION\n" );
				m_iNumberRound = 0;
				m_iRoundTime = gpGlobals->time + 5;
				m_iRoundState = 5;
			}
			else
			{
				UTIL_ClientPrintAll( HUD_PRINTCENTER, "FAILED ROUND\n" );
				m_iRoundTime = gpGlobals->time + 5;
				m_iRoundState = 0;
			}
		}
		else
		{
			if ( m_iNumberRound >= 10 )
			{
				m_FailMissionGo = 1;
				m_Missionpasbon = 1;
				UTIL_ClientPrintAll( HUD_PRINTCENTER, "FAILED MISSION\n" );
				m_iNumberRound = 0;
				m_iRoundTime = gpGlobals->time + 5;
				m_iRoundState = 5;
			}
			else
			{
				UTIL_ClientPrintAll( HUD_PRINTCENTER, "FAILED ROUND\n" );
				m_iRoundTime = gpGlobals->time + 5;
				m_iRoundState = 0;
			}
		}
	}
	else
	{
		if ( RoundLimit == 1 )
		{
			if ( m_iNumberRound >= m_iMaxRound )
			{
				m_FailMissionGo = 1;
				m_Missionpasbon = 1;
				UTIL_ClientPrintAll( HUD_PRINTCENTER, "FAILED MISSION\n" );
				m_iNumberRound = 0;
				m_iRoundTime = gpGlobals->time + 5;
				m_iRoundState = 5;
			}
			else
			{
				UTIL_ClientPrintAll( HUD_PRINTCENTER, "FAILED ROUND\n" );
				m_iRoundTime = gpGlobals->time + 5;
				m_iRoundState = 0;
			}
		}
		else
		{
			if ( m_iNumberRound >= 7 )
			{
				m_FailMissionGo = 1;
				m_Missionpasbon = 1;
				UTIL_ClientPrintAll( HUD_PRINTCENTER, "FAILED MISSION\n" );
				m_iNumberRound = 0;
				m_iRoundTime = gpGlobals->time + 5;
				m_iRoundState = 5;
			}
			else
			{
				UTIL_ClientPrintAll( HUD_PRINTCENTER, "FAILED ROUND\n" );
				m_iRoundTime = gpGlobals->time + 5;
				m_iRoundState = 0;
			}
		}
	}
}

//=========================================================
//=========================================================
void CCoop::CheckRound( void )
{
	m_iRoundState = 4;
	m_iRoundTime = 0;
}

//=========================================================
//=========================================================
void CCoop::MissionEnd( void )
{
	m_FailMissionGo = 0;
	m_iParticip1 = 0;
	m_iLive1 = 0;
	m_VoteYes = 0;
	m_VoteNo = 0;

	if ( m_Missionpasbon == 1 )
	{
		m_Missionpasbon = 0;
		m_iRoundState = 2;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( pPlayer && !pPlayer->m_iSbo2a8 )
			{
				if ( pPlayer->pev->weapons )
					pPlayer->RemoveAllItems( TRUE );

				m_iParticip1++;
				ShowVGUI( pPlayer, 18 );
			}
		}
	}
	else
	{
		m_Missionpasbon = 0;
		m_iRoundState = 0;
		GoToIntermission();
	}
}

//=========================================================
//=========================================================
void CCoop::SetupRound( void )
{
	m_iParticip1 = 0;
	m_iLive1 = 0;
	m_iDeathLimit = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer && !pPlayer->m_iSbo2a8 )
		{
			if ( pPlayer->pev->weapons )
				pPlayer->RemoveAllItems( TRUE );

			m_iParticip1++;

			if ( pPlayer->m_iSbo2e8 == 1 )
				m_iLive1++;
		}
	}

	if ( m_iLive1 < m_iParticip1 )
	{
		m_iParticip1 = 0;
		m_iLive1 = 0;
		m_iDeathLimit = 0;
		m_iRoundTime = gpGlobals->time + 20;
		m_iRoundState = 0;
	}
	else
	{
		CheckWho();
	}
}

//=========================================================
//=========================================================
void CCoop::CheckWho( void )
{
	m_iParticip1 = 0;
	m_iLive1 = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer && !pPlayer->m_iSbo2a8 )
		{
			if ( pPlayer->pev->weapons )
				pPlayer->RemoveAllItems( TRUE );

			m_iParticip1++;

			if ( pPlayer->m_iSbo2e8 == 1 )
			{
				m_iLive1++;
				pPlayer->m_iSbo2ec = 2;
				EntSelectSpawnPoint( pPlayer );
				pPlayer->StopObserver();
				pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
				pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
				pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
				pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
				pPlayer->m_iSpectator = 0;
				UTIL_ClientPrintAll( HUD_PRINTCENTER, "ROUND START\n" );
				pPlayer->Spawn();
			}
		}
	}

	m_iNumberRound++;
	m_iDeathLimit = m_iLive1;
	m_iRoundTime = gpGlobals->time + 0;
	m_iRoundState = 3;
}

//=========================================================
//=========================================================
void CCoop::CheckRoundOver( void )
{
	if ( m_iParticip1 >= 1 )
	{
		int hardcore = CVAR_GET_FLOAT( "mp_hardcore" );

		if ( m_iLive1 <= m_iDeathLimit - hardcore * m_iDeathLimit / 100 )
		{
			m_iParticip1 = 0;
			m_iLive1 = 0;
			m_iDeathLimit = 0;
			m_iRoundTime = 0;
			m_iRoundState = 1;
		}
	}
}

//=========================================================
//=========================================================
BOOL CCoop::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if ( g_VoiceGameMgr.ClientCommand( pPlayer, pcmd ) )
		return TRUE;

	if ( FStrEq( pcmd, "menuselect" ) )
	{
		if ( CMD_ARGC() < 2 )
			return TRUE;

		int slot = atoi( CMD_ARGV(1) );

		switch ( pPlayer->m_iSbo318 )
		{
		case 2:
			switch ( slot )
			{
			case 1:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->KillThemAll();
				pPlayer->m_iSbo318 = 2;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm" );
				break;
			case 2:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->Imp();
				pPlayer->m_iSbo318 = 2;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm" );
				break;
			case 3:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->Imphunter();
				pPlayer->m_iSbo318 = 2;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm" );
				break;
			case 4:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->Hunter();
				pPlayer->m_iSbo318 = 2;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm" );
				break;
			case 5:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->Hunter2();
				pPlayer->m_iSbo318 = 2;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm" );
				break;
			case 6:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->Dark();
				pPlayer->m_iSbo318 = 2;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm" );
				break;
			case 7:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->Hgrunt();
				pPlayer->m_iSbo318 = 2;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm" );
				break;
			case 8:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->Dgrunt();
				pPlayer->m_iSbo318 = 2;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm" );
				break;
			case 9:
				pPlayer->m_iSbo318 = 3;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm2" );
				break;
			case 10:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
				{
					ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "gm mode off" );
					GmEngage = 0;
					pPlayer->m_iSbo304 = 0;
					pPlayer->m_iSbo2ec = 1;
					pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
					pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
					pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
					pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
					pPlayer->m_iSpectator = 1;
					pPlayer->m_iSbo2a8 = 0;
					pPlayer->m_iSbo2e8 = 1;
				}
				else
				{
					pPlayer->m_iSbo318 = 2;
					pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm" );
				}
				break;
			}
			break;

		case 3:
			switch ( slot )
			{
			case 1:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->KillThemAll();
				pPlayer->m_iSbo318 = 3;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm2" );
				break;
			case 2:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->Assassin();
				pPlayer->m_iSbo318 = 3;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm2" );
				break;
			case 3:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->Barney();
				pPlayer->m_iSbo318 = 3;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm2" );
				break;
			case 4:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->Minos();
				pPlayer->m_iSbo318 = 3;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm2" );
				break;
			case 5:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->Gargan();
				pPlayer->m_iSbo318 = 3;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm2" );
				break;
			case 6:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->Apache();
				pPlayer->m_iSbo318 = 3;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm2" );
				break;
			case 7:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->Zombie();
				pPlayer->m_iSbo318 = 3;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm2" );
				break;
			case 8:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
					pPlayer->MZombie();
				pPlayer->m_iSbo318 = 3;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm2" );
				break;
			case 9:
				pPlayer->m_iSbo318 = 2;
				pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm" );
				break;
			case 10:
				if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 1 && GmEngage == 1 )
				{
					ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "gm mode off" );
					GmEngage = 0;
					pPlayer->m_iSbo304 = 0;
					pPlayer->m_iSbo2ec = 1;
					pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
					pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
					pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
					pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
					pPlayer->m_iSpectator = 1;
					pPlayer->m_iSbo2a8 = 0;
					pPlayer->m_iSbo2e8 = 1;
				}
				else
				{
					pPlayer->m_iSbo318 = 3;
					pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm2" );
				}
				break;
			}
			break;

		case 0:
			if ( !pPlayer->m_iSpectator )
			{
				if ( pPlayer->m_iSbo2cc == 1 )
				{
					if ( slot == 1 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/cover.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Cover Me !\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 2 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/follow.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Follow Me!!\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 3 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/shoot.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Shoot Them !\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 4 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/medic.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Medic !!!!!!!!!\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 5 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/help.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Help Me !!!!\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 6 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/wait.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Wait Me !!!\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 7 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/side.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Attack by side\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 8 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/grenade.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Grenade !!\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 9 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/retreat.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Retreat !!!\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
				}
				else
				{
					if ( slot == 1 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/coverf.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Cover Me !\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 2 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/followf.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Follow Me!!\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 3 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/shootf.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Shoot Them !\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 4 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/medicf.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Medic !!!!!!!!!\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 5 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/helpf.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Help Me !!!!\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 6 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/waitf.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Wait Me !!!\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 7 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/sidef.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Attack by side\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 8 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/grenadef.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Grenade !!\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
					else if ( slot == 9 )
					{
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								EMIT_SOUND( pPlayer->edict(), CHAN_STATIC, "radio/retreatf.wav", 1, ATTN_NORM );
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Retreat !!!\n", STRING( pPlayer->pev->netname ) ), pEnt );
							}
						}
					}
				}
			}
			break;
		}

		return TRUE;
	}

	if ( FStrEq( pcmd, "vote_yes" ) )
	{
		m_VoteYes++;
		return TRUE;
	}

	if ( FStrEq( pcmd, "vote_no" ) )
	{
		m_VoteNo++;
		return TRUE;
	}

	if ( FStrEq( pcmd, "gmon" ) )
	{
		if ( gmmode.value == 1 && pPlayer->m_iSbo304 == 0 && GmEngage == 0 )
		{
			ShowVGUI( pPlayer, 19 );
			ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "gm mode on" );
			GmEngage = 1;
			pPlayer->m_iSbo304 = 1;
			pPlayer->m_iSbo2ec = 1;
			pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
			pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
			pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
			pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
			pPlayer->m_iSpectator = 1;
			pPlayer->m_iSbo2a8 = 1;
			pPlayer->m_iSbo318 = 2;
			pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Gmm" );

			if ( m_iRoundState == 3 )
			{
				m_iParticip1--;
				if ( !pPlayer->m_iSpectator )
					m_iLive1--;
				CheckRoundOver();
			}
			else
			{
				m_iParticip1--;
				CheckRound();
			}

			pPlayer->m_iSbo2e8 = 0;
			respawn( pPlayer->pev, FALSE );
		}
		else if ( GmEngage == 1 )
		{
			ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "there is already a gm" );
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "changemodel" ) )
	{
		ShowVGUI( pPlayer, 10 );
		return TRUE;
	}

	if ( FStrEq( pcmd, "radio" ) )
	{
		if ( !pPlayer->m_iSpectator && m_Mode == MODE_COOP )
		{
			pPlayer->m_iSbo318 = 0;
			pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Radio" );
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "malefemale" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iSbo2ac = 1;
			break;
		case 2:
			pPlayer->m_iSbo2ac = 2;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "blade" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iSbo2b0 = 1;
			break;
		case 2:
			pPlayer->m_iSbo2b0 = 2;
			break;
		case 3:
			pPlayer->m_iSbo2b0 = 3;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "light" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iSbo2b4 = 1;
			break;
		case 2:
			pPlayer->m_iSbo2b4 = 2;
			break;
		case 3:
			pPlayer->m_iSbo2b4 = 3;
			break;
		case 4:
			pPlayer->m_iSbo2b4 = 4;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "heavy" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			if ( pPlayer->m_iSbo2b8 == 9 )
				m_iCoopParticip--;
			if ( pPlayer->m_iSbo2b8 == 7 )
				m_iCoopLive--;
			pPlayer->m_iSbo2b8 = 1;
			break;
		case 2:
			if ( pPlayer->m_iSbo2b8 == 9 )
				m_iCoopParticip--;
			if ( pPlayer->m_iSbo2b8 == 7 )
				m_iCoopLive--;
			pPlayer->m_iSbo2b8 = 2;
			break;
		case 3:
			if ( pPlayer->m_iSbo2b8 == 9 )
				m_iCoopParticip--;
			if ( pPlayer->m_iSbo2b8 == 7 )
				m_iCoopLive--;
			pPlayer->m_iSbo2b8 = 3;
			break;
		case 4:
			if ( pPlayer->m_iSbo2b8 == 9 )
				m_iCoopParticip--;
			if ( pPlayer->m_iSbo2b8 == 7 )
				m_iCoopLive--;
			pPlayer->m_iSbo2b8 = 4;
			break;
		case 5:
			if ( pPlayer->m_iSbo2b8 == 9 )
				m_iCoopParticip--;
			if ( pPlayer->m_iSbo2b8 == 7 )
				m_iCoopLive--;
			pPlayer->m_iSbo2b8 = 5;
			break;
		case 6:
			if ( pPlayer->m_iSbo2b8 == 9 )
				m_iCoopParticip--;
			if ( pPlayer->m_iSbo2b8 == 7 )
				m_iCoopLive--;
			pPlayer->m_iSbo2b8 = 6;
			break;
		case 7:
			if ( pPlayer->m_iSbo2b8 == 9 )
				m_iCoopParticip--;
			if ( m_iCoopLive < 3 )
			{
				if ( pPlayer->m_iSbo2b8 == 7 )
				{
					pPlayer->m_iSbo2b8 = 7;
				}
				else
				{
					m_iCoopLive++;
					pPlayer->m_iSbo2b8 = 7;
				}
			}
			else
			{
				ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "2 Grenades launchers max / team" );
			}
			break;
		case 8:
			if ( pPlayer->m_iSbo2b8 == 9 )
				m_iCoopParticip--;
			if ( pPlayer->m_iSbo2b8 == 7 )
				m_iCoopLive--;
			pPlayer->m_iSbo2b8 = 8;
			break;
		case 9:
			if ( pPlayer->m_iSbo2b8 == 7 )
				m_iCoopLive--;
			if ( m_iCoopParticip < 2 )
			{
				if ( pPlayer->m_iSbo2b8 == 9 )
				{
					pPlayer->m_iSbo2b8 = 9;
				}
				else
				{
					m_iCoopParticip++;
					pPlayer->m_iSbo2b8 = 9;
				}
			}
			else
			{
				ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "2 Nuclear launchers max / team" );
			}
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "armor" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iSbo2bc = 1;
			break;
		case 2:
			pPlayer->m_iSbo2bc = 2;
			break;
		case 3:
			pPlayer->m_iSbo2bc = 3;
			break;
		case 4:
			pPlayer->m_iSbo2bc = 0;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "cyberware" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iSbo2c0 = 1;
			break;
		case 2:
			pPlayer->m_iSbo2c0 = 2;
			break;
		case 3:
			pPlayer->m_iSbo2c0 = 3;
			break;
		case 4:
			pPlayer->m_iSbo2c0 = 4;
			break;
		case 5:
			pPlayer->m_iSbo2c0 = 5;
			break;
		case 6:
			pPlayer->m_iSbo2c0 = 6;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "item" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iSbo2c4 = 1;
			if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
			{
				pPlayer->m_iSbo2ec = 1;
				pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
				pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
				pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
				pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
				pPlayer->m_iSpectator = 1;
				pPlayer->m_iSbo2e8 = 1;
				respawn( pPlayer->pev, FALSE );
			}
			break;
		case 2:
			pPlayer->m_iSbo2c4 = 2;
			if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
			{
				pPlayer->m_iSbo2ec = 1;
				pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
				pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
				pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
				pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
				pPlayer->m_iSpectator = 1;
				pPlayer->m_iSbo2e8 = 1;
				respawn( pPlayer->pev, FALSE );
			}
			break;
		case 3:
			pPlayer->m_iSbo2c4 = 3;
			if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
			{
				pPlayer->m_iSbo2ec = 1;
				pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
				pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
				pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
				pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
				pPlayer->m_iSpectator = 1;
				pPlayer->m_iSbo2e8 = 1;
				respawn( pPlayer->pev, FALSE );
			}
			break;
		case 4:
			pPlayer->m_iSbo2c4 = 4;
			if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
			{
				pPlayer->m_iSbo2ec = 1;
				pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
				pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
				pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
				pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
				pPlayer->m_iSpectator = 1;
				pPlayer->m_iSbo2e8 = 1;
				respawn( pPlayer->pev, FALSE );
			}
			break;
		}
		return TRUE;
	}

	return FALSE;
}

//=========================================================
//=========================================================
void CCoop::PlayerSpawn( CBasePlayer *pPlayer )
{
	BOOL		addDefault;
	CBaseEntity	*pWeaponEntity = NULL;

	pPlayer->pev->weapons |= (1<<WEAPON_SUIT);
	
	addDefault = TRUE;

	while ( pWeaponEntity = UTIL_FindEntityByClassname( pWeaponEntity, "game_player_equip" ))
	{
		pWeaponEntity->Touch( pPlayer );
		addDefault = FALSE;
	}

	if ( pPlayer->m_iSbo2ec == 0 )
	{
		pPlayer->StopObserver();
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "Waiting for a carnage..." );
		pPlayer->pev->classname = MAKE_STRING( "observer" );
		pPlayer->pev->solid = SOLID_NOT;
		pPlayer->pev->takedamage = DAMAGE_NO;
		pPlayer->pev->movetype = MOVETYPE_NONE;
		pPlayer->pev->flags |= FL_NOTARGET;
		pPlayer->pev->effects |= EF_NODRAW;
		pPlayer->m_afPhysicsFlags |= PFLAG_OBSERVER;
		pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT | HIDEHUD_HEALTH;

		if ( m_Mode == MODE_DM || m_Mode == MODE_BR || m_Mode == MODE_COOP )
			pPlayer->m_iSpectator = 1;

		addDefault = FALSE;
	}
	else if ( pPlayer->m_iSbo2ec == 1 )
	{
		pPlayer->m_iSpectator = 1;
		edict_t *pentSpawnSpot = EntSelectSpawnPoint( pPlayer );
		pPlayer->StartObserver( VARS(pentSpawnSpot)->origin, VARS(pentSpawnSpot)->angles );
	}
	else
	{
		pPlayer->StopObserver();
		pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
		pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
		pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
		pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
		pPlayer->m_iSpectator = 0;
	}

	if ( addDefault )
		pPlayer->GiveNamedItem( "weapon_wong" );

	pPlayer->m_iSbo2f4 = gpGlobals->time + 2;
	pPlayer->pev->fov = pPlayer->m_iFOV = 90;
	pPlayer->pev->movetype = MOVETYPE_WALK;
	pPlayer->m_iSbo2d4 = 0;

	if ( ban.value == 1 && pPlayer->m_iSbo2fc >= 5 )
	{
		pPlayer->RemoveAllItems( TRUE );
		pPlayer->pev->health = 3;
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "you are a teamKiller" );
		pPlayer->m_iSbo2c0 = 0;
		pPlayer->m_iSbo2bc = 0;
		pPlayer->m_iSbo2c4 = 0;
		pPlayer->m_iSbo2b4 = 0;
		pPlayer->m_iSbo2b8 = 0;
		pPlayer->m_iSbo2b0 = 0;
		pPlayer->pev->weapons |= (1<<WEAPON_SUIT);
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "hunted" );
		pPlayer->m_iSbo2ac = 0;
		pPlayer->m_iSbo2cc = 1;
	}

	if ( pPlayer->m_iSbo2c0 == 5 )
		pPlayer->m_iCanCloak = 1;
	else
		pPlayer->m_iCanCloak = 0;

	if ( pPlayer->m_iSbo2c0 == 6 || pPlayer->m_iSbo2c0 == 5 )
		pPlayer->m_iCanCybereye = 1;
	else
		pPlayer->m_iCanCybereye = 0;

	if ( pPlayer->m_iSbo2c0 == 2 || pPlayer->m_iSbo2c0 == 5 )
		pPlayer->m_iSbo2e0 = 1;
	else
		pPlayer->m_iSbo2e0 = 0;

	pPlayer->pev->renderamt = 255;
	pPlayer->pev->renderfx = kRenderFxNone;
	pPlayer->pev->rendermode = kRenderNormal;
	pPlayer->pev->gravity = 1;
	pPlayer->m_iSboMaxSpeed = pPlayer->m_iSboSpeed;

	switch ( pPlayer->m_iSbo2ac )
	{
	case 1:
		pPlayer->m_iSbo2cc = 0;
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agentf" );
		break;
	case 2:
		pPlayer->m_iSbo2cc = 1;
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agent" );
		break;
	default:
		pPlayer->m_iSbo2cc = 1;
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agent" );
		break;
	}

	switch ( pPlayer->m_iSbo2bc )
	{
	case 1:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->pev->armorvalue = 50;
		pPlayer->m_iSbo2d4 = 3;
		break;
	case 2:
		pPlayer->m_iSboMaxSpeed -= 15;
		pPlayer->pev->armorvalue = 100;
		pPlayer->m_iSbo2d4 = 1;
		break;
	case 3:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->pev->armorvalue = 100;
		pPlayer->m_iSbo2d4 = 2;
		break;
	}

	switch ( pPlayer->m_iSbo2c0 )
	{
	case 1:
		pPlayer->m_iSboMaxSpeed += 50;
		break;
	case 2:
		pPlayer->m_iSbo2e0 = 1;
		break;
	case 3:
		pPlayer->GiveNamedItem( "weapon_medikit" );
		break;
	case 4:
		pPlayer->pev->health = pPlayer->m_iSboHealth + 75;
		break;
	case 5:
		pPlayer->m_iSbo2b4 = 0;
		pPlayer->m_iSbo2b8 = 0;
		pPlayer->m_iSbo2c0 = 5;
		pPlayer->m_iSbo2c4 = 0;
		pPlayer->GiveNamedItem( "weapon_K444" );
		pPlayer->m_iCanCloak = 1;
		pPlayer->m_iCanCybereye = 1;
		pPlayer->m_iSbo2e0 = 1;
		pPlayer->GiveNamedItem( "weapon_medikit" );
		pPlayer->pev->armorvalue = 50;
		pPlayer->m_iSbo2d4 = 3;
		pPlayer->m_iSboMaxSpeed += 25;
		pPlayer->pev->health = pPlayer->m_iSboHealth + 50;
		pPlayer->GiveNamedItem( "weapon_handgrenade" );
		pPlayer->GiveNamedItem( "weapon_satchel" );
		break;
	case 6:
		pPlayer->m_iCanCybereye = 1;
		break;
	}

	switch ( pPlayer->m_iSbo2c4 )
	{
	case 1:
		pPlayer->GiveNamedItem( "weapon_handgrenade" );
		break;
	case 2:
		pPlayer->m_iSboMaxSpeed -= 6;
		pPlayer->GiveNamedItem( "weapon_tripmine" );
		break;
	case 3:
		pPlayer->m_iSboMaxSpeed -= 15;
		pPlayer->GiveNamedItem( "weapon_satchel" );
		break;
	case 4:
		pPlayer->GiveNamedItem( "weapon_snark" );
		break;
	}

	switch ( pPlayer->m_iSbo2b0 )
	{
	case 1:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->GiveNamedItem( "weapon_warhammer" );
		break;
	case 2:
		pPlayer->GiveNamedItem( "weapon_chainsaw" );
		break;
	case 3:
		pPlayer->m_iSboMaxSpeed -= 15;
		pPlayer->GiveNamedItem( "weapon_sword" );
		break;
	}

	switch ( pPlayer->m_iSbo2b4 )
	{
	case 1:
		pPlayer->GiveNamedItem( "weapon_rc28" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Rc28, "rc28m", 900 );
		else
			pPlayer->GiveAmmo( 120, "rc28m", 900 );
		break;
	case 2:
		pPlayer->GiveNamedItem( "weapon_K444" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_K444, "K444m", 900 );
		else
			pPlayer->GiveAmmo( 140, "K444m", 900 );
		break;
	case 3:
		pPlayer->m_iSboMaxSpeed -= 15;
		pPlayer->GiveNamedItem( "weapon_canon" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Canon, "canonm", 900 );
		else
			pPlayer->GiveAmmo( 180, "canonm", 900 );
		break;
	case 4:
		pPlayer->m_iSboMaxSpeed -= 15;
		pPlayer->GiveNamedItem( "weapon_pm66" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Pm66, "pm66m", 900 );
		else
			pPlayer->GiveAmmo( 150, "pm66m", 900 );
		break;
	}

	switch ( pPlayer->m_iSbo2b8 )
	{
	case 1:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->GiveNamedItem( "weapon_fa18" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Fa18, "fa18m", 900 );
		else
			pPlayer->GiveAmmo( 250, "fa18m", 900 );
		break;
	case 2:
		pPlayer->m_iSboMaxSpeed -= 20;
		pPlayer->GiveNamedItem( "weapon_federal" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Federal, "federalm", 900 );
		else
			pPlayer->GiveAmmo( 210, "federalm", 900 );
		break;
	case 3:
		pPlayer->m_iSboMaxSpeed -= 45;
		pPlayer->GiveNamedItem( "weapon_sw" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Sw, "swm", 900 );
		else
			pPlayer->GiveAmmo( 50, "swm", 900 );
		break;
	case 4:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->GiveNamedItem( "weapon_nm33" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Nm33, "nm33m", 900 );
		else
			pPlayer->GiveAmmo( 150, "nm33m", 900 );
		break;
	case 5:
		pPlayer->m_iSboMaxSpeed -= 20;
		pPlayer->GiveNamedItem( "weapon_spa" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Spa, "spam", 900 );
		else
			pPlayer->GiveAmmo( 160, "spam", 900 );
		break;
	case 6:
		pPlayer->m_iSboMaxSpeed -= 20;
		pPlayer->GiveNamedItem( "weapon_ben" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Ben, "benm", 900 );
		else
			pPlayer->GiveAmmo( 120, "benm", 900 );
		break;
	case 7:
		pPlayer->m_iSboMaxSpeed -= 60;
		pPlayer->GiveNamedItem( "weapon_glauncher" );
		pPlayer->GiveAmmo( 1, "ARgrenades", 20 );
		break;
	case 8:
		pPlayer->m_iSboMaxSpeed -= 80;
		pPlayer->GiveNamedItem( "weapon_chaingun" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Mini, "chainm", 900 );
		else
			pPlayer->GiveAmmo( 100, "chainm", 900 );
		break;
	case 9:
		pPlayer->m_iSboMaxSpeed -= 80;
		pPlayer->GiveNamedItem( "weapon_rpg" );
		break;
	}

	if ( pPlayer->m_iSbo304 == 1 )
		pPlayer->m_iSboMaxSpeed = 1000;

	g_engfuncs.pfnSetClientMaxspeed( pPlayer->edict(), pPlayer->m_iSboMaxSpeed );
}

//=========================================================
//=========================================================
BOOL CCoop::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	pPlayer->m_iSpectator = 1;

	edict_t *pentSpawnSpot = EntSelectSpawnPoint( pPlayer );
	pPlayer->StartObserver( VARS(pentSpawnSpot)->origin, VARS(pentSpawnSpot)->angles );

	return FALSE;
}

//=========================================================
//=========================================================
int CCoop::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return -5;
}

//=========================================================
//=========================================================
// The 1.38 release's hl.dll and hl_i386.so were built from different SBO source snapshots (NOTES.md, "Server
// binaries: release vs production"): hl.dll (like the production hl_i386.so) applies the teamkill/self-kill
// m_iSboSpeed/m_iSboHealth penalties only when the XP cvar is 1; the release hl_i386.so applies them always.
void CCoop::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	DeathNotice( pVictim, pKiller, pInflictor );

	if ( m_iRoundState == 3 )
	{
		pVictim->m_iSbo2ec = 1;
		m_iLive1--;
		pVictim->m_iSpectator = 1;
		CheckRoundOver();
	}

	pVictim->m_iDeaths += 1;
	pVictim->m_iSbo308 = pVictim->m_iSbo308 / 2;

	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );
	CBasePlayer *peKiller = NULL;
	CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );
	if ( ktmp && (ktmp->Classify() == CLASS_PLAYER) )
		peKiller = (CBasePlayer*)ktmp;

	if ( pVictim->pev == pKiller )
	{  // killed self
		if ( peKiller && peKiller->Classify() == CLASS_PLAYER )
		{
			if ( pKiller->frags <= 4 )
				pKiller->frags = 0;
			else
				pKiller->frags -= 5;

#ifdef _WIN32
			if ( peKiller->m_iSboSpeed <= 110 )
			{
				if ( XP.value == 1 )
					peKiller->m_iSboSpeed = 100;
			}
			else
			{
				if ( XP.value == 1 )
					peKiller->m_iSboSpeed -= 25;
			}

			if ( peKiller->m_iSboHealth <= 26 )
			{
				if ( XP.value == 1 )
					peKiller->m_iSboHealth = 10;
			}
			else
			{
				if ( XP.value == 1 )
					peKiller->m_iSboHealth -= 25;
			}
#else
			if ( peKiller->m_iSboSpeed <= 110 )
				peKiller->m_iSboSpeed = 100;
			else
				peKiller->m_iSboSpeed -= 25;

			if ( peKiller->m_iSboHealth <= 26 )
				peKiller->m_iSboHealth = 10;
			else
				peKiller->m_iSboHealth -= 25;
#endif
		}
	}
	else if ( ktmp && ktmp->IsPlayer() )
	{
		if ( peKiller && peKiller->Classify() == CLASS_PLAYER )
		{
			if ( pKiller->frags <= 4 )
				pKiller->frags = 0;
			else
				pKiller->frags -= 5;

#ifdef _WIN32
			if ( peKiller->m_iSboSpeed <= 110 )
			{
				if ( XP.value == 1 )
					peKiller->m_iSboSpeed = 100;
			}
			else
			{
				if ( XP.value == 1 )
					peKiller->m_iSboSpeed -= 25;
			}

			if ( peKiller->m_iSboHealth <= 26 )
			{
				if ( XP.value == 1 )
					peKiller->m_iSboHealth = 10;
			}
			else
			{
				if ( XP.value == 1 )
					peKiller->m_iSboHealth -= 25;
			}
#else
			if ( peKiller->m_iSboSpeed <= 110 )
				peKiller->m_iSboSpeed = 100;
			else
				peKiller->m_iSboSpeed -= 25;

			if ( peKiller->m_iSboHealth <= 26 )
				peKiller->m_iSboHealth = 10;
			else
				peKiller->m_iSboHealth -= 25;
#endif
		}

		peKiller->m_iSbo2fc++;

		FireTargets( "game_playerkill", ktmp, ktmp, USE_TOGGLE, 0 );
	}

	// update the scores
	// killed scores
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pVictim->edict()) );
		WRITE_SHORT( pVictim->pev->frags );
		WRITE_SHORT( pVictim->m_iDeaths );
		WRITE_SHORT( 0 );
		WRITE_SHORT( GetTeamIndex( pVictim->m_szTeamName ) + 1 );
	MESSAGE_END();

	// killers score, if it's a player
	CBaseEntity *ep = CBaseEntity::Instance( pKiller );
	if ( ep && ep->Classify() == CLASS_PLAYER )
	{
		CBasePlayer *PK = (CBasePlayer*)ep;

		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(PK->edict()) );
			WRITE_SHORT( PK->pev->frags );
			WRITE_SHORT( PK->m_iDeaths );
			WRITE_SHORT( 0 );
			WRITE_SHORT( GetTeamIndex( PK->m_szTeamName) + 1 );
		MESSAGE_END();

		// let the killer paint another decal as soon as he'd like.
		PK->m_flNextDecalTime = gpGlobals->time;
	}

	if ( pVictim->HasNamedPlayerItem("weapon_satchel") )
	{
		DeactivateSatchels( pVictim );
	}

#ifdef _WIN32
	// ARTIFICIAL (hl.dll layout only): VC6 emits the pending inline CCoop::PlayTextureSounds (coop.h) once enough
	// front-end work has accumulated in this file; hl.dll has it before FPlayerCanTakeDamage, so SBO's production
	// source had more code-free front-end work than this reconstruction (~70 dead `if` statements' worth, anywhere
	// before FPlayerCanTakeDamage; its nature is unknown). These generate no code. See NOTES.md, hl.dll H5.
	if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { }
	if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { }
	if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { }
	if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { }
	if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { }
	if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { }
	if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { }
	if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { }
	if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { }
	if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { } if ( 0 ) { }
#endif
}

//=========================================================
//=========================================================
BOOL CCoop::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if ( friendlyfire.value == 0 && pAttacker != pPlayer && pAttacker->IsPlayer() )
		return FALSE;

	return TRUE;
}
