//=========================================================
// SBO 2.0: per-client queued senders (client_sendqueue.h)
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"client_sendqueue.h"

struct sendqueue_sender_t
{
	SENDQUEUE_FN	pfn;	// NULL: no sender with this id
	int				start;
	float			delay;
	float			interval;
	int				bytes;
};

// zero before any static constructor runs, so the registration order doesn't matter
static sendqueue_sender_t s_Senders[ SENDQUEUE_MAX ];

SendQueue_Sender::SendQueue_Sender( int id, SENDQUEUE_FN pfn, int iStart, float flDelay, float flInterval, int iBytes )
{
	if ( id < 0 || id >= SENDQUEUE_MAX )
		return;

	s_Senders[id].pfn = pfn;
	s_Senders[id].start = iStart;
	s_Senders[id].delay = flDelay;
	s_Senders[id].interval = flInterval;
	s_Senders[id].bytes = iBytes;
}

void SendQueue_Start( CBasePlayer *pPlayer, int id )
{
	if ( id < 0 || id >= SENDQUEUE_MAX || !s_Senders[id].pfn )
		return;

	sendqueue_state_t *q = &pPlayer->m_SendQueue[id];
	q->sending = TRUE;
	q->cursor = s_Senders[id].start;
	q->nextSend = gpGlobals->time + s_Senders[id].delay;
}

void SendQueue_StartAll( CBasePlayer *pPlayer )
{
	for ( int id = 0; id < SENDQUEUE_MAX; id++ )
		SendQueue_Start( pPlayer, id );
}

void SendQueue_Run( CBasePlayer *pPlayer )
{
	for ( int id = 0; id < SENDQUEUE_MAX; id++ )
	{
		sendqueue_state_t *q = &pPlayer->m_SendQueue[id];
		if ( !q->sending || gpGlobals->time < q->nextSend )
			continue;

		if ( !s_Senders[id].pfn || ( pPlayer->pev->flags & FL_FAKECLIENT ) )
		{
			q->sending = FALSE;
			continue;
		}

		int iBudget = s_Senders[id].bytes;
		if ( s_Senders[id].pfn( pPlayer, q->cursor, iBudget ) )
			q->sending = FALSE;
		else
			q->nextSend = gpGlobals->time + s_Senders[id].interval;
	}
}
