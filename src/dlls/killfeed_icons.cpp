//=========================================================
// SBO 2.0: killfeed icons set by server plugins (docs/KILLFEED_ICONS.md, game_shared/killfeed_icons_shared.h).
//
// Server command (plugins: server_cmd, amxmodx include/killfeed_icons.inc):
//   killfeed_icon <code> <sprites/sheet.spr> <x> <y> <w> <h>   the icon is that rectangle of the sheet
//   killfeed_icon <code> <other code>                           the icon of d_<other code>
//   killfeed_icon <code>                                        remove
//   killfeed_icon                                               list
// <code> is the weapon code of DeathMsg / SboDeathMsg. The plugin precaches the sheet (precache_generic). The table
// is emptied at map end (ServerDeactivate); plugins set their icons every map.
//
// Clients: one "KfIcon" message to all on a change, and the table to a client whose HUD was (re)initialised (the
// send queue, client_sendqueue.h).
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"client_sendqueue.h"
#include	"../game_shared/killfeed_icons_shared.h"

extern int gmsgKfIcon;
extern bool g_worldExists;	// cbase.cpp: a map is running

#define KFICON_QUEUE_DELAY		0.5
#define KFICON_QUEUE_INTERVAL	0.1
#define KFICON_QUEUE_BYTES		300
#define KFICON_MSG_HEADER		3		// message id + size byte, as client_sendqueue users count it

typedef struct
{
	char	code[KFICON_CODE_MAX + 1];	// "" = free
	int		kind;						// KFICON_SHEET / KFICON_REUSE
	char	source[KFICON_PATH_LEN];	// sheet path or the other icon's code
	int		x, y, w, h;
} kficon_t;

static kficon_t s_KfIcons[KFICON_MAX];

static kficon_t *KfIcon_Find( const char *code )
{
	for ( int i = 0; i < KFICON_MAX; i++ )
		if ( s_KfIcons[i].code[0] && !strcmp( s_KfIcons[i].code, code ) )
			return &s_KfIcons[i];
	return NULL;
}

static int KfIcon_Bytes( const kficon_t *icon )
{
	int n = KFICON_MSG_HEADER + 1 + (int)strlen( icon->code ) + 1 + (int)strlen( icon->source ) + 1;
	return icon->kind == KFICON_SHEET ? n + 8 : n;
}

static void KfIcon_Write( const kficon_t *icon )
{
	WRITE_BYTE( icon->kind );
	WRITE_STRING( icon->code );
	WRITE_STRING( icon->source );
	if ( icon->kind == KFICON_SHEET )
	{
		WRITE_SHORT( icon->x );
		WRITE_SHORT( icon->y );
		WRITE_SHORT( icon->w );
		WRITE_SHORT( icon->h );
	}
}

static void KfIcon_SendAll( const kficon_t *icon )
{
	if ( !gmsgKfIcon || !g_worldExists )
		return;	// no map running (console): the clients get the table at their HUD init
	MESSAGE_BEGIN( MSG_ALL, gmsgKfIcon );
		KfIcon_Write( icon );
	MESSAGE_END();
}

static void KfIcon_Usage( void )
{
	g_engfuncs.pfnServerPrint( "Usage: killfeed_icon <code> <sprites/sheet.spr> <x> <y> <w> <h>\n"
		"       killfeed_icon <code> <other code>   (use that icon)\n"
		"       killfeed_icon <code>                (remove)\n"
		"       killfeed_icon                       (list)\n" );
}

static void KfIcon_List( void )
{
	int n = 0;
	for ( int i = 0; i < KFICON_MAX; i++ )
	{
		const kficon_t *icon = &s_KfIcons[i];
		if ( !icon->code[0] )
			continue;
		if ( icon->kind == KFICON_SHEET )
			g_engfuncs.pfnServerPrint( UTIL_VarArgs( "  d_%s: %s %d %d %d %d\n", icon->code, icon->source,
				icon->x, icon->y, icon->w, icon->h ) );
		else
			g_engfuncs.pfnServerPrint( UTIL_VarArgs( "  d_%s: d_%s\n", icon->code, icon->source ) );
		n++;
	}
	g_engfuncs.pfnServerPrint( UTIL_VarArgs( "%d killfeed icons (at most %d)\n", n, KFICON_MAX ) );
}

static bool KfIcon_ReadNumber( const char *arg, int lo, int hi, int &out )
{
	if ( !arg || !arg[0] )
		return false;
	for ( const char *p = arg; *p; p++ )
		if ( *p < '0' || *p > '9' || p - arg > 5 )
			return false;
	out = atoi( arg );
	return out >= lo && out <= hi;
}

static void KfIcon_Command( void )
{
	const int argc = CMD_ARGC();
	if ( argc <= 1 )
	{
		KfIcon_List();
		return;
	}

	const char *code = CMD_ARGV( 1 );
	if ( !KfIcon_ValidCode( code ) )
	{
		g_engfuncs.pfnServerPrint( UTIL_VarArgs( "killfeed_icon: bad code \"%s\" (1-%d of A-Z a-z 0-9 _ -)\n",
			code, KFICON_CODE_MAX ) );
		return;
	}

	kficon_t icon;
	memset( &icon, 0, sizeof( icon ) );
	strcpy( icon.code, code );	// checked: at most KFICON_CODE_MAX characters

	if ( argc == 2 )
	{
		kficon_t *old = KfIcon_Find( code );
		if ( !old )
			return;
		old->kind = KFICON_REMOVE;
		old->source[0] = 0;
		KfIcon_SendAll( old );
		memset( old, 0, sizeof( *old ) );
		return;
	}

	if ( argc == 3 )
	{
		const char *other = CMD_ARGV( 2 );
		if ( !KfIcon_ValidCode( other ) || !strcmp( other, code ) )
		{
			g_engfuncs.pfnServerPrint( UTIL_VarArgs( "killfeed_icon: bad icon code \"%s\"\n", other ) );
			return;
		}
		icon.kind = KFICON_REUSE;
		strcpy( icon.source, other );
	}
	else if ( argc == 7 )
	{
		const char *path = CMD_ARGV( 2 );
		if ( !KfIcon_ValidPath( path ) )
		{
			g_engfuncs.pfnServerPrint( UTIL_VarArgs( "killfeed_icon: bad sheet \"%s\" (sprites/....spr, under %d characters)\n",
				path, KFICON_PATH_LEN ) );
			return;
		}
		if ( !KfIcon_ReadNumber( CMD_ARGV( 3 ), 0, 4095, icon.x ) || !KfIcon_ReadNumber( CMD_ARGV( 4 ), 0, 4095, icon.y )
			|| !KfIcon_ReadNumber( CMD_ARGV( 5 ), 1, KFICON_RECT_MAX, icon.w )
			|| !KfIcon_ReadNumber( CMD_ARGV( 6 ), 1, KFICON_RECT_MAX, icon.h ) )
		{
			g_engfuncs.pfnServerPrint( UTIL_VarArgs( "killfeed_icon: bad rectangle (x, y 0-4095; w, h 1-%d)\n",
				KFICON_RECT_MAX ) );
			return;
		}
		icon.kind = KFICON_SHEET;
		strcpy( icon.source, path );
	}
	else
	{
		KfIcon_Usage();
		return;
	}

	kficon_t *slot = KfIcon_Find( code );
	if ( slot && slot->kind == icon.kind && !strcmp( slot->source, icon.source ) && slot->x == icon.x
		&& slot->y == icon.y && slot->w == icon.w && slot->h == icon.h )
		return;	// unchanged (a plugin sets its icons again)
	if ( !slot )
	{
		for ( int i = 0; i < KFICON_MAX && !slot; i++ )
			if ( !s_KfIcons[i].code[0] )
				slot = &s_KfIcons[i];
		if ( !slot )
		{
			g_engfuncs.pfnServerPrint( UTIL_VarArgs( "killfeed_icon: table full (%d icons)\n", KFICON_MAX ) );
			return;
		}
	}
	*slot = icon;
	KfIcon_SendAll( slot );
}

void KillFeedIcons_Init( void )
{
	g_engfuncs.pfnAddServerCommand( (char *)"killfeed_icon", KfIcon_Command );
}

// ServerDeactivate: plugins set their icons again on the next map
void KillFeedIcons_Reset( void )
{
	memset( s_KfIcons, 0, sizeof( s_KfIcons ) );
}

// the send queue (client_sendqueue.h, SENDQUEUE_KFICON): every icon, from iCursor on
static int KfIcon_SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget )
{
	for ( int i = max( iCursor, 0 ); i < KFICON_MAX; i++ )
	{
		const kficon_t *icon = &s_KfIcons[i];
		if ( !icon->code[0] )
			continue;
		const int bytes = KfIcon_Bytes( icon );
		if ( bytes > iBudget )
		{
			iCursor = i;
			return FALSE;
		}
		MESSAGE_BEGIN( MSG_ONE, gmsgKfIcon, NULL, pPlayer->edict() );
			KfIcon_Write( icon );
		MESSAGE_END();
		iBudget -= bytes;
	}
	return TRUE;
}

static SendQueue_Sender s_KfIconSender( SENDQUEUE_KFICON, KfIcon_SendQueued, 0, KFICON_QUEUE_DELAY,
	KFICON_QUEUE_INTERVAL, KFICON_QUEUE_BYTES );
