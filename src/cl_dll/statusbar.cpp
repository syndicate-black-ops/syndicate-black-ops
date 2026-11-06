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
//
// statusbar.cpp
//
// generic text status bar, set by game dll
// runs across bottom of screen
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui2/vgui2_api.h"

#include <string.h>
#include <stdio.h>

DECLARE_MESSAGE( m_StatusBar, StatusText );
DECLARE_MESSAGE( m_StatusBar, StatusValue );

#define STATUSBAR_ID_LINE		1

float *GetClientColor( int clientIndex );
extern float g_ColorYellow[3];

int CHudStatusBar :: Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( StatusText );
	HOOK_MESSAGE( StatusValue );

	Reset();

	CVAR_CREATE( "hud_centerid", "0", FCVAR_ARCHIVE );

	return 1;
}

int CHudStatusBar :: VidInit( void )
{
	// Load sprites here

	return 1;
}

void CHudStatusBar :: Reset( void )
{
	int i = 0;

	m_iFlags &= ~HUD_ACTIVE;  // start out inactive
	for ( i = 0; i < MAX_STATUSBAR_LINES; i++ )
		m_szStatusText[i][0] = 0;
	memset( m_iStatusValues, 0, sizeof m_iStatusValues );

	m_iStatusValues[0] = 1;  // 0 is the special index, which always returns true

	// reset our colors for the status bar lines (yellow is default)
	for ( i = 0; i < MAX_STATUSBAR_LINES; i++ )
	{
		m_pflNameColors[i] = g_ColorYellow;
		m_bNameColorCodes[i] = false;
	}
	memset( m_iNameClients, 0, sizeof m_iNameClients );
	m_bHasNames = false;
}

void CHudStatusBar :: ParseStatusString( int line_num )
{
	// localise string first
	char szBuffer[MAX_STATUSTEXT_LENGTH];
	memset( szBuffer, 0, sizeof szBuffer );
	gHUD.m_TextMessage.LocaliseTextString( m_szStatusText[line_num], szBuffer, MAX_STATUSTEXT_LENGTH );

	// parse m_szStatusText & m_iStatusValues into m_szStatusBar
	memset( m_szStatusBar[line_num], 0, MAX_STATUSTEXT_LENGTH );
	memset( m_iNameClients[line_num], 0, MAX_STATUSTEXT_LENGTH );
	m_bNameColorCodes[line_num] = false;
	char *src = szBuffer;
	char *dst = m_szStatusBar[line_num];

	char *src_start = src, *dst_start = dst;

	while ( *src != 0 )
	{
		while ( *src == '\n' )
			src++;  // skip over any newlines

		if ( ((src - src_start) >= MAX_STATUSTEXT_LENGTH) || ((dst - dst_start) >= MAX_STATUSTEXT_LENGTH - 1) )	// -1: matches the copy loops below
			break;

		int index = atoi( src );
		// should we draw this line?
		if ( (index >= 0 && index < MAX_STATUSBAR_VALUES) && (m_iStatusValues[index] != 0) )
		{  // parse this line and append result to the status bar
			while ( *src >= '0' && *src <= '9' )
				src++;

			if ( *src == '\n' || *src == 0 )
				continue; // no more left in this text line

			// copy the text, char by char, until we hit a % or a \n
			// Robustness: keep room for the terminator; substitutions could push plain text past the end of the line
			while ( *src != '\n' && *src != 0 && (dst - dst_start) < MAX_STATUSTEXT_LENGTH - 1 )
			{
				if ( *src != '%' )
				{  // just copy the character
					*dst = *src;
					dst++, src++;
				}
				else
				{
					// get the descriptor
					char valtype = *(++src); // move over %

					// if it's a %, draw a % sign
					if ( valtype == '%' )
					{
						*dst = valtype;
						dst++, src++;
						continue;
					}

					// move over descriptor, then get and move over the index
					index = atoi( ++src ); 
					while ( *src >= '0' && *src <= '9' )
						src++;

					if ( index >= 0 && index < MAX_STATUSBAR_VALUES )
					{
						int indexval = m_iStatusValues[index];

						// get the string to substitute in place of the %XX
						char szRepString[MAX_STATUSTEXT_LENGTH];
						int iNameClient = 0;	// SBO: the player whose name szRepString is
						switch ( valtype )
						{
						case 'p':  // player name
							// Robustness: the value is a short from the server and indexes g_PlayerInfoList[MAX_PLAYERS+1]
							if ( indexval < 1 || indexval > MAX_PLAYERS )
							{
								strcpy( szRepString, "******" );
								break;
							}
							GetPlayerInfo( indexval, &g_PlayerInfoList[indexval] );
							if ( g_PlayerInfoList[indexval].name != NULL )
							{
								// SBO: the name as typed; its color codes are drawn as colors with hud_colortext 1 (DrawStatusLine),
								// else removed below. The name color is not in the text: DrawStatusLine reads it from his userinfo.
								strncpy( szRepString, g_PlayerInfoList[indexval].name, MAX_STATUSTEXT_LENGTH );
								szRepString[MAX_STATUSTEXT_LENGTH - 1] = 0;
								m_pflNameColors[line_num] = GetClientColor( indexval );
								m_bNameColorCodes[line_num] = ( VGui2_Chat_GetColorCodeAction() == 1 );
								iNameClient = indexval;
								m_bHasNames = true;
							}
							else
							{
								strcpy( szRepString, "******" );
							}

							break;
						case 'i':  // number
							sprintf( szRepString, "%d", indexval );
							break;
						default:
							szRepString[0] = 0;
						}

						// SBO: a name's chars are marked with its player (DrawStatusLine); a color code in it is copied
						// whole or not at all with hud_colortext 1, else removed (the 8-bit HUD text never shows a code)
						for ( char *cp = szRepString; *cp != 0; )
						{
							int len = iNameClient ? VGui2_Chat_GetColorCode( cp, NULL, NULL ) : 0;
							if ( len > 0 && !m_bNameColorCodes[line_num] )
							{
								cp += len;
								continue;
							}
							if ( len <= 0 )
								len = 1;
							if ( (dst - dst_start) + len > MAX_STATUSTEXT_LENGTH - 1 )
								break;
							for ( ; len > 0; len--, cp++, dst++ )
							{
								*dst = *cp;
								m_iNameClients[line_num][dst - dst_start] = (unsigned char)iNameClient;
							}
						}
					}
				}
			}
		}
		else
		{
			// skip to next line of text
			while ( *src != 0 && *src != '\n' )
				src++;
		}
	}
}

// SBO: draws line line_num at x, y in segments, or with bDraw false only measures it; returns the end x. A player's
// name (m_iNameClients) is drawn as the chat and kill feed draw it: with hud_colortext 1 in his name color (the
// "namecolor" key of his userinfo, VGui2_Chat_GetPlayerNameColor; none: his team color), the codes typed in the name
// switching the color and ^0 / ^9 going back to the name color; else in his team color. The rest of the line is
// drawn in the line's color (m_pflNameColors). The engine resets the text color after each string, so it is set
// before each segment.
int CHudStatusBar :: DrawStatusLine( int line_num, int x, int y, bool bDraw )
{
	const char *text = m_szStatusBar[line_num];
	const unsigned char *clients = m_iNameClients[line_num];
	char seg[MAX_STATUSTEXT_LENGTH];
	int i = 0;

	while ( text[i] != 0 )
	{
		const int client = clients[i];
		const float *base = client ? GetClientColor( client ) : m_pflNameColors[line_num];
		float nameColor[3];
		int rgbName[3];
		if ( client && m_bNameColorCodes[line_num] && VGui2_Chat_GetPlayerNameColor( client, rgbName ) )
		{
			nameColor[0] = rgbName[0] / 255.0f, nameColor[1] = rgbName[1] / 255.0f, nameColor[2] = rgbName[2] / 255.0f;
			base = nameColor;
		}
		float color[3] = { 0, 0, 0 };
		if ( base )
			color[0] = base[0], color[1] = base[1], color[2] = base[2];

		int n = 0;
		for ( ;; )
		{
			const bool bEnd = ( text[i] == 0 ) || ( clients[i] != client );
			bool bReset = true;
			int rgb[3] = { 0, 0, 0 };
			int codeLen = 0;
			if ( !bEnd && client && m_bNameColorCodes[line_num] )
				codeLen = VGui2_Chat_GetColorCode( text + i, &bReset, rgb );

			if ( !bEnd && codeLen <= 0 )
			{
				seg[n++] = text[i++];
				continue;
			}

			// end of the segment: draw (or measure) it in its color
			if ( n > 0 )
			{
				seg[n] = 0;
				if ( bDraw )
				{
					if ( base )
						gEngfuncs.pfnDrawSetTextColor( color[0], color[1], color[2] );
					x = DrawConsoleString( x, y, seg );
				}
				else
				{
					int w, h;
					GetConsoleStringSize( seg, &w, &h );
					x += w;
				}
				n = 0;
			}

			if ( bEnd )
				break;

			// a color code: its color, or back to the name's own color
			if ( bReset )
			{
				if ( base )
					color[0] = base[0], color[1] = base[1], color[2] = base[2];
			}
			else
			{
				color[0] = rgb[0] / 255.0f, color[1] = rgb[1] / 255.0f, color[2] = rgb[2] / 255.0f;
			}
			i += codeLen;
		}
	}

	return x;
}

int CHudStatusBar :: Draw( float fTime )
{
	// SBO: a line with a player name is parsed every frame, so the name, his name color and hud_colortext are current
	if ( m_bReparseString || m_bHasNames )
	{
		m_bHasNames = false;
		for ( int i = 0; i < MAX_STATUSBAR_LINES; i++ )
		{
			m_pflNameColors[i] = g_ColorYellow;
			ParseStatusString( i );
		}
		m_bReparseString = FALSE;
	}

	// SBO 2.0 GM mode: nothing but the Game Master's own HUD while he flies
	if ( VGui2_GmHudOwnsScreen() )
		return 1;

	int Y_START = gHUD.GetHeight() - HUD_YRES(32 + 4);	// HUD scaling: HUD units

	// Draw the status bar lines
	for ( int i = 0; i < MAX_STATUSBAR_LINES; i++ )
	{
		int TextHeight, TextWidth;
		GetConsoleStringSize( m_szStatusBar[i], &TextWidth, &TextHeight );
		TextWidth = DrawStatusLine( i, 0, 0, false );	// SBO: without the names' color codes

		int x = 4;
		int y = Y_START - ( 4 + TextHeight * i ); // draw along bottom of screen

		// let user set status ID bar centering
		if ( (i == STATUSBAR_ID_LINE) && CVAR_GET_FLOAT("hud_centerid") )
		{
			x = max( 0, max(2, (gHUD.GetWidth() - TextWidth)) / 2 );
			y = (gHUD.GetHeight() / 2) + (TextHeight*CVAR_GET_FLOAT("hud_centerid"));
		}

		// SBO: player names in their name color, the rest in the line's color (DrawStatusLine)
		DrawStatusLine( i, x, y, true );
	}

	return 1;
}

// Message handler for StatusText message
// accepts two values:
//		byte: line number of status bar text 
//		string: status bar text
// this string describes how the status bar should be drawn
// a semi-regular expression:
// ( slotnum ([a..z] [%pX] [%iX])*)*
// where slotnum is an index into the Value table (see below)
// if slotnum is 0, the string is always drawn
// if StatusValue[slotnum] != 0, the following string is drawn, upto the next newline - otherwise the text is skipped upto next newline
// %pX, where X is an integer, will substitute a player name here, getting the player index from StatusValue[X]
// %iX, where X is an integer, will substitute a number here, getting the number from StatusValue[X]
int CHudStatusBar :: MsgFunc_StatusText( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int line = READ_BYTE();

	if ( line < 0 || line >= MAX_STATUSBAR_LINES )
		return 1;

	strncpy( m_szStatusText[line], READ_STRING(), MAX_STATUSTEXT_LENGTH );
	m_szStatusText[line][MAX_STATUSTEXT_LENGTH-1] = 0;  // ensure it's null terminated ( strncpy() won't null terminate if read string too long)

	// halflife-updated: the old "m_szStatusText[0] == 0" test compared an array with 0 and was never true
	m_iFlags |= HUD_ACTIVE;

	m_bReparseString = TRUE;

	return 1;
}

// Message handler for StatusText message
// accepts two values:
//		byte: index into the status value array
//		short: value to store
int CHudStatusBar :: MsgFunc_StatusValue( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int index = READ_BYTE();
	if ( index < 1 || index >= MAX_STATUSBAR_VALUES )
		return 1; // index out of range

	m_iStatusValues[index] = READ_SHORT();

	m_bReparseString = TRUE;
	
	return 1;
}