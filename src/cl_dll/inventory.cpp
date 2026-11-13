//=========================================================
// SBO 2.0 inventory, client side: the HUD row (docs/INVENTORY.md). The message and the lists: inventory_core.cpp.
//
// Left of the screen, below the middle (the status icons grow up from the middle): a row of the player's own
// items, then a row of the team's shared items ("TEAM" in front), each an icon from sprites/hud.txt (the item's
// item_icon, INVENTORY_DEFAULT_ICON when hud.txt has none of that name) and "xN" after it when N is above 1.
// Icons keep their own colors (drawn additive with white). Nothing is drawn for a spectator, a dead or waiting
// player (an observer: the HUD's health is hidden then), in an intermission, or while the HUD is hidden.
//=========================================================

#include <string.h>
#include <stdio.h>

#include "hud.h"
#include "cl_util.h"
#include "inventory.h"
#include "inventory_core.h"

static inventory::Lists s_Lists;
static int s_Sprites[2][ INVENTORY_SLOTS ];	// hud.txt index of each item's icon (-1: none)

static void Inventory_ResolveSprites( void )
{
	const int iDefault = gHUD.GetSpriteIndex( INVENTORY_DEFAULT_ICON );
	for ( int l = 0; l < 2; l++ )
	{
		const inventory::Item *list = l ? s_Lists.shared : s_Lists.own;
		for ( int i = 0; i < INVENTORY_SLOTS; i++ )
		{
			int idx = -1;
			if ( list[i].name[0] )
			{
				if ( list[i].icon[0] )
					idx = gHUD.GetSpriteIndex( list[i].icon );
				if ( idx < 0 )
					idx = iDefault;
			}
			s_Sprites[l][i] = idx;
		}
	}
}

static int MsgFunc_Inventory( const char *pszName, int iSize, void *pbuf )
{
	inventory::Msg msg;
	if ( !inventory::Parse( (const unsigned char *)pbuf, iSize, msg ) )
		return 1;
	if ( inventory::Apply( s_Lists, msg ) )
		Inventory_ResolveSprites();
	return 1;
}

void Inventory_Init( void )
{
	gEngfuncs.pfnHookUserMsg( INVENTORY_MSG_NAME, MsgFunc_Inventory );
	Inventory_Reset();
}

void Inventory_Reset( void )
{
	inventory::Clear( s_Lists );
	memset( s_Sprites, -1, sizeof( s_Sprites ) );
}

void Inventory_VidInit( void )
{
	Inventory_ResolveSprites();	// hud.txt's sprites were loaded again
}

// one row; returns its height (0: nothing drawn)
static int Inventory_DrawRow( int x, int y, int l, const char *pszLabel )
{
	const inventory::Item *list = l ? s_Lists.shared : s_Lists.own;
	const int n = inventory::Used( list );
	if ( !n )
		return 0;

	int r, g, b;
	UnpackRGB( r, g, b, RGB_YELLOWISH );

	int iHeight = gHUD.m_iFontHeight;
	for ( int i = 0; i < n; i++ )
	{
		const int idx = s_Sprites[l][i];
		if ( idx >= 0 )
		{
			const wrect_t &rc = gHUD.GetSpriteRect( idx );
			if ( rc.bottom - rc.top > iHeight )
				iHeight = rc.bottom - rc.top;
		}
	}

	const int iTextY = y + ( iHeight - gHUD.m_iFontHeight ) / 2 + 2;
	if ( pszLabel )
		x = gHUD.DrawHudString( x, iTextY, gHUD.GetWidth(), (char *)pszLabel, r, g, b ) + 6;

	for ( int i = 0; i < n; i++ )
	{
		const int idx = s_Sprites[l][i];
		if ( idx >= 0 && gHUD.GetSprite( idx ) )
		{
			const wrect_t &rc = gHUD.GetSpriteRect( idx );
			SPR_Set( gHUD.GetSprite( idx ), 255, 255, 255 );
			SPR_DrawAdditive( 0, x, y + ( iHeight - ( rc.bottom - rc.top ) ) / 2, &rc );
			x += rc.right - rc.left;
		}
		if ( list[i].count > 1 )
		{
			char sz[16];
			snprintf( sz, sizeof( sz ), "x%d", list[i].count );
			x = gHUD.DrawHudString( x + 2, iTextY, gHUD.GetWidth(), sz, r, g, b );
		}
		x += 8;
	}
	return iHeight;
}

void Inventory_Redraw( float flTime )
{
	if ( gEngfuncs.IsSpectateOnly() || g_iUser1 || ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_ALL | HIDEHUD_HEALTH ) ) )
		return;	// spectators, observers (dead or waiting players), HLTV: no inventory row
	if ( !( gHUD.m_iWeaponBits & ( 1 << WEAPON_SUIT ) ) )
		return;	// no HUD without the suit (as the health and armor)

	int x = 5;
	int y = gHUD.GetHeight() / 2 + 16;
	const int h = Inventory_DrawRow( x, y, 0, NULL );
	if ( h )
		y += h + 4;
	Inventory_DrawRow( x, y, 1, "TEAM" );
}
