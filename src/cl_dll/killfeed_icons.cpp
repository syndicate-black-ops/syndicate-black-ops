//=========================================================
// SBO 2.0: killfeed icons set by server plugins, client side (docs/KILLFEED_ICONS.md,
// game_shared/killfeed_icons_shared.h). The server's "KfIcon" messages fill a table of up to KFICON_MAX icons; after
// every change the table is put into the HUD sprite list again as d_<code> entries after hud.txt's
// (CHud::AddExtraSprite). The classic kill feed (death.cpp) and the VGUI2 one (vgui2/hud_deathnotice.cpp) look up
// d_<weapon code> there, so they show the icon with no change of their own.
//
// A sheet the client does not have (downloads off, missing on the server): the icon is left out and the kill shows
// the skull, as before.
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "killfeed_icons.h"
#include "killfeed_icons_shared.h"

#include <string.h>

typedef struct
{
	char	code[KFICON_CODE_MAX + 1];	// "" = free
	int		kind;						// KFICON_SHEET / KFICON_REUSE
	char	other[KFICON_CODE_MAX + 1];	// KFICON_REUSE: the other icon's code
	HSPRITE	hSprite;					// KFICON_SHEET: 0 = the sheet could not be loaded
	wrect_t	rc;
} kficon_client_t;

static kficon_client_t s_Icons[KFICON_MAX];

static kficon_client_t *KfIcon_Find( const char *code )
{
	for ( int i = 0; i < KFICON_MAX; i++ )
		if ( s_Icons[i].code[0] && !strcmp( s_Icons[i].code, code ) )
			return &s_Icons[i];
	return NULL;
}

// the sheet if the client has it (SPR_Load on a missing file is not tried); the engine reuses a loaded sprite
static HSPRITE KfIcon_LoadSheet( const char *path )
{
	int length = 0;
	byte *pFile = gEngfuncs.COM_LoadFile( (char *)path, 5, &length );
	if ( !pFile )
	{
		gEngfuncs.Con_DPrintf( "KfIcon: %s missing, the skull is shown instead\n", path );
		return 0;
	}
	gEngfuncs.COM_FreeFile( pFile );
	if ( length < 8 )
		return 0;
	return SPR_Load( path );
}

// the HUD sprite list's d_ entries again from the table: sheets first, then the reuses (a reuse of a reuse too)
static void KfIcon_Apply( void )
{
	gHUD.ClearExtraSprites();

	bool added[KFICON_MAX] = {};
	char name[MAX_SPRITE_NAME_LENGTH];
	for ( int i = 0; i < KFICON_MAX; i++ )
	{
		const kficon_client_t *icon = &s_Icons[i];
		if ( !icon->code[0] || icon->kind != KFICON_SHEET || !icon->hSprite )
			continue;
		_snprintf( name, sizeof( name ), "d_%s", icon->code );
		name[sizeof( name ) - 1] = 0;
		gHUD.AddExtraSprite( name, icon->hSprite, icon->rc );
		added[i] = true;
	}

	for ( int pass = 0; pass < KFICON_MAX; pass++ )
	{
		bool progress = false;
		for ( int i = 0; i < KFICON_MAX; i++ )
		{
			const kficon_client_t *icon = &s_Icons[i];
			if ( added[i] || !icon->code[0] || icon->kind != KFICON_REUSE )
				continue;
			_snprintf( name, sizeof( name ), "d_%s", icon->other );
			name[sizeof( name ) - 1] = 0;
			const int index = gHUD.GetSpriteIndex( name );
			if ( index < 0 || !gHUD.GetSprite( index ) )
				continue;	// not there (yet)
			const HSPRITE hSprite = gHUD.GetSprite( index );
			const wrect_t rc = gHUD.GetSpriteRect( index );
			_snprintf( name, sizeof( name ), "d_%s", icon->code );
			name[sizeof( name ) - 1] = 0;
			gHUD.AddExtraSprite( name, hSprite, rc );
			added[i] = progress = true;
		}
		if ( !progress )
			break;
	}
}

static int MsgFunc_KfIcon( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	const int kind = READ_BYTE();
	char code[KFICON_CODE_MAX + 1];
	strncpy( code, READ_STRING(), sizeof( code ) - 1 );
	code[sizeof( code ) - 1] = 0;
	char source[KFICON_PATH_LEN];
	strncpy( source, READ_STRING(), sizeof( source ) - 1 );
	source[sizeof( source ) - 1] = 0;
	int x = 0, y = 0, w = 0, h = 0;
	if ( kind == KFICON_SHEET )
	{
		x = READ_SHORT();
		y = READ_SHORT();
		w = READ_SHORT();
		h = READ_SHORT();
	}
	if ( !READ_OK() || !KfIcon_ValidCode( code ) )
		return 1;

	kficon_client_t *icon = KfIcon_Find( code );
	if ( kind == KFICON_REMOVE )
	{
		if ( icon )
		{
			memset( icon, 0, sizeof( *icon ) );
			KfIcon_Apply();
		}
		return 1;
	}

	kficon_client_t entry;
	memset( &entry, 0, sizeof( entry ) );
	strcpy( entry.code, code );
	entry.kind = kind;
	if ( kind == KFICON_SHEET )
	{
		if ( !KfIcon_ValidPath( source ) || x < 0 || y < 0 || w < 1 || h < 1 || w > KFICON_RECT_MAX || h > KFICON_RECT_MAX )
			return 1;
		entry.hSprite = KfIcon_LoadSheet( source );
		if ( entry.hSprite )
		{
			// the rectangle inside the sprite's first frame
			const int sw = SPR_Width( entry.hSprite, 0 );
			const int sh = SPR_Height( entry.hSprite, 0 );
			if ( x >= sw || y >= sh )
				entry.hSprite = 0;
			entry.rc.left = x;
			entry.rc.top = y;
			entry.rc.right = min( x + w, sw );
			entry.rc.bottom = min( y + h, sh );
		}
	}
	else if ( kind == KFICON_REUSE )
	{
		if ( !KfIcon_ValidCode( source ) )
			return 1;
		strcpy( entry.other, source );
	}
	else
		return 1;

	if ( !icon )
	{
		for ( int i = 0; i < KFICON_MAX && !icon; i++ )
			if ( !s_Icons[i].code[0] )
				icon = &s_Icons[i];
		if ( !icon )
			return 1;	// full (the server keeps at most as many)
	}
	*icon = entry;
	KfIcon_Apply();
	return 1;
}

void KillFeedIcons_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "KfIcon", MsgFunc_KfIcon );
	memset( s_Icons, 0, sizeof( s_Icons ) );
}

void KillFeedIcons_Reset( void )
{
	memset( s_Icons, 0, sizeof( s_Icons ) );
	gHUD.ClearExtraSprites();
}
