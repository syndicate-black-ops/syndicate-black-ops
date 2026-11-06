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
// saytext.cpp
//
// implementation of CHudSayText class
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>

#include "viewport.h"
#include "vgui2/vgui2_api.h"	// SBO 2.0: VGui2_Chat_RemoveColorCodes

extern float *GetClientColor( int clientIndex );

#define MAX_LINES	5
#define MAX_CHARS_PER_LINE	256  /* it can be less than this, depending on char size */

// allow 20 pixels on either side of the text
#define MAX_LINE_WIDTH  ( gHUD.GetWidth() - 40 )	// HUD scaling: HUD units
#define LINE_START  10
static float SCROLL_SPEED = 5;

static char g_szLineBuffer[ MAX_LINES + 1 ][ MAX_CHARS_PER_LINE ];
static float *g_pflNameColors[ MAX_LINES + 1 ];
static int g_iNameLengths[ MAX_LINES + 1 ];
static int g_iNameStarts[ MAX_LINES + 1 ];		// chat-vgui2: name offset (after a "*DEAD* " style tag), 0/1 none
static bool g_bNoConsole[ MAX_LINES + 1 ];		// chat-vgui2: transient line (hit message), not copied to the console
static float flScrollTime = 0;  // the time at which the lines next scroll up

static int Y_START = 0;
static int line_height = 0;

int StringLength( const char *psz );

// chat-vgui2: shared with the VGUI2 chat (declared in vgui2/vgui2_api.h)
//
// The server's hit messages (player.cpp TraceAttack, HUD_PRINTTALK): shown in the chat area but never kept in the
// chat history or copied to the console. Exact text, trailing whitespace/newlines ignored (works with old servers).
static const char *s_pszSboHitMessages[] =
{
	"My fucking head",
	"My chest !!!",
	"Arg !!! my stomach !!",
	"This is my arm fucker !",
	"Oh no my leg !",
};

bool SboChat_IsTransientMessage( const char *pszMsg )
{
	if ( !pszMsg )
		return false;

	int len = (int)strlen( pszMsg );
	while ( len > 0 && ( pszMsg[len - 1] == '\n' || pszMsg[len - 1] == '\r' || pszMsg[len - 1] == ' ' ) )
		len--;

	for ( int i = 0; i < (int)( sizeof( s_pszSboHitMessages ) / sizeof( s_pszSboHitMessages[0] ) ); i++ )
	{
		if ( (int)strlen( s_pszSboHitMessages[i] ) == len && !strncmp( pszMsg, s_pszSboHitMessages[i], len ) )
			return true;
	}
	return false;
}

// Length of the server's scoreboard-category tag ("*DEAD* ", "*SPEC* ", "*NOLOAD* ", "*GM* ") at psz, else 0. The
// chats look for the sender's name after it, so only the name takes the team colour (the tag is plain text).
int SboChat_TagLength( const char *psz )
{
	static const char *s_pszTags[] = { "*DEAD* ", "*SPEC* ", "*NOLOAD* ", "*GM* " };

	if ( !psz )
		return 0;

	for ( int i = 0; i < (int)( sizeof( s_pszTags ) / sizeof( s_pszTags[0] ) ); i++ )
	{
		int len = (int)strlen( s_pszTags[i] );
		if ( !strncmp( psz, s_pszTags[i], len ) )
			return len;
	}
	return 0;
}

DECLARE_MESSAGE( m_SayText, SayText );

int CHudSayText :: Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( SayText );

	InitHUDData();

	m_HUD_saytext =			gEngfuncs.pfnRegisterVariable( "hud_saytext", "1", FCVAR_ARCHIVE );	// SBO 2.0: kept, as the VGUI2 chat's
	m_HUD_saytext_time =	gEngfuncs.pfnRegisterVariable( "hud_saytext_time", "5", FCVAR_ARCHIVE );	// SBO 2.0: archived, as the VGUI2 chat's
	m_con_color =			gEngfuncs.pfnGetCvarPointer( "con_color" );

	m_iFlags |= HUD_INTERMISSION; // is always drawn during an intermission

	return 1;
}


void CHudSayText :: InitHUDData( void )
{
	memset( g_szLineBuffer, 0, sizeof g_szLineBuffer );
	memset( g_pflNameColors, 0, sizeof g_pflNameColors );
	memset( g_iNameLengths, 0, sizeof g_iNameLengths );
	memset( g_iNameStarts, 0, sizeof g_iNameStarts );
	memset( g_bNoConsole, 0, sizeof g_bNoConsole );
}

int CHudSayText :: VidInit( void )
{
	return 1;
}


int ScrollTextUp( void )
{
	if ( !g_bNoConsole[0] )	// chat-vgui2: hit messages stay out of the console
		ConsolePrint( g_szLineBuffer[0] ); // move the first line into the console buffer
	g_szLineBuffer[MAX_LINES][0] = 0;
	memmove( g_szLineBuffer[0], g_szLineBuffer[1], sizeof(g_szLineBuffer) - sizeof(g_szLineBuffer[0]) ); // overwrite the first line
	memmove( &g_pflNameColors[0], &g_pflNameColors[1], sizeof(g_pflNameColors) - sizeof(g_pflNameColors[0]) );
	memmove( &g_iNameLengths[0], &g_iNameLengths[1], sizeof(g_iNameLengths) - sizeof(g_iNameLengths[0]) );
	memmove( &g_iNameStarts[0], &g_iNameStarts[1], sizeof(g_iNameStarts) - sizeof(g_iNameStarts[0]) );
	memmove( &g_bNoConsole[0], &g_bNoConsole[1], sizeof(g_bNoConsole) - sizeof(g_bNoConsole[0]) );
	g_bNoConsole[MAX_LINES - 1] = false;
	g_szLineBuffer[MAX_LINES-1][0] = 0;

	if ( g_szLineBuffer[0][0] == ' ' ) // also scroll up following lines
	{
		g_szLineBuffer[0][0] = 2;
		return 1 + ScrollTextUp();
	}

	return 1;
}

int CHudSayText :: Draw( float flTime )
{
	int y = Y_START;

	if ( !m_HUD_saytext->value )	// chat-vgui2: no AllowedToPrintText() suppression
		return 1;

	// make sure the scrolltime is within reasonable bounds,  to guard against the clock being reset
	flScrollTime = min( flScrollTime, flTime + m_HUD_saytext_time->value );

	// make sure the scrolltime is within reasonable bounds,  to guard against the clock being reset
	flScrollTime = min( flScrollTime, flTime + m_HUD_saytext_time->value );

	if ( flScrollTime <= flTime )
	{
		if ( *g_szLineBuffer[0] )
		{
			flScrollTime = flTime + m_HUD_saytext_time->value;
			// push the console up
			ScrollTextUp();
		}
		else
		{ // buffer is empty,  just disable drawing of this section
			m_iFlags &= ~HUD_ACTIVE;
		}
	}

	// halflife-updated (halflife #3240): set the text color to con_color before drawing so chat lines always use
	// the same color; the engine resets the color to that value only after drawing a string.
	int r, g, b;
	if ( m_con_color && m_con_color->string && sscanf( m_con_color->string, "%i %i %i", &r, &g, &b ) == 3 )
		gEngfuncs.pfnDrawSetTextColor( r / 255.0f, g / 255.0f, b / 255.0f );

	for ( int i = 0; i < MAX_LINES; i++ )
	{
		if ( *g_szLineBuffer[i] )
		{
			if ( *g_szLineBuffer[i] == 2 && g_pflNameColors[i] )
			{
				// it's a saytext string
				static char buf[MAX_PLAYER_NAME_LENGTH+32];

				// chat-vgui2: a "*DEAD* " style tag before the name is drawn in the normal colour
				int x = LINE_START;
				int iStart = 0;
				if ( g_iNameStarts[i] > 1 && g_iNameStarts[i] < g_iNameLengths[i] )
				{
					iStart = min( g_iNameStarts[i], MAX_PLAYER_NAME_LENGTH+31 );
					strncpy( buf, g_szLineBuffer[i], iStart );
					buf[iStart] = 0;
					x = DrawConsoleString( x, y, buf );
				}

				// draw the first x characters in the player color
				int iLen = min( g_iNameLengths[i] - iStart, MAX_PLAYER_NAME_LENGTH+31 );
				strncpy( buf, g_szLineBuffer[i] + iStart, iLen );
				buf[iLen] = 0;
				gEngfuncs.pfnDrawSetTextColor( g_pflNameColors[i][0], g_pflNameColors[i][1], g_pflNameColors[i][2] );
				x = DrawConsoleString( x, y, buf );

				// color is reset after each string draw
				DrawConsoleString( x, y, g_szLineBuffer[i] + g_iNameLengths[i] );
			}
			else
			{
				// normal draw
				DrawConsoleString( LINE_START, y, g_szLineBuffer[i] );
			}
		}

		y += line_height;
	}


	return 1;
}

int CHudSayText :: MsgFunc_SayText( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int client_index = READ_BYTE();		// the client who spoke the message
	SayTextPrint( READ_STRING(), iSize - 1,  client_index );
	
	return 1;
}

void CHudSayText :: SayTextPrint( const char *pszBuf, int iBufSize, int clientIndex )
{
	// chat-vgui2 branch: everyone always sees all chat. SBO 1.38 filtered here
	// (dead players only saw "*DEAD*" messages, living players only the others; with fullscreen
	// VGUI1 menus up the text went to the console only). IsDead() is kept but unused.

	// chat-vgui2: the new server sends "\x07RRGGBB<name>\x01: msg" (name coloured by state) and players may type colour
	// codes. This legacy chat can't draw them: a leading hex colour becomes \x02 (team-coloured name below) and
	// other colour markup is dropped.
	char szClean[256];
	{
		const char *in = pszBuf;
		char *out = szClean;
		char *end = szClean + sizeof( szClean ) - 1;
		if ( *in == 7 ) { *out++ = 2; in++; for ( int h = 0; h < 6 && *in; h++ ) in++; }
		while ( *in && out < end )
		{
			if ( *in == 7 ) { in++; for ( int h = 0; h < 6 && *in; h++ ) in++; continue; }
			if ( *in == 8 ) { in++; for ( int h = 0; h < 8 && *in; h++ ) in++; continue; }
			if ( out != szClean && *in >= 1 && *in <= 6 ) { in++; continue; }
			*out++ = *in++;
		}
		*out = 0;

		// SBO 2.0: nor can it draw ^0-^9 / ^xRGB / ^#RRGGBB codes (names, typed text): removed with the chat's parser
		// (kept as typed with hud_colortext 0). The name is looked up in the line without its codes too (below).
		const char *pszNoCodes = VGui2_Chat_RemoveColorCodes( szClean );
		if ( pszNoCodes != szClean )
		{
			memmove( szClean, pszNoCodes, min( strlen( pszNoCodes ) + 1, sizeof( szClean ) ) );
			szClean[sizeof( szClean ) - 1] = 0;
		}

		pszBuf = szClean;
		iBufSize = sizeof( szClean );
	}

	// find an empty string slot
	int i; for ( i = 0; i < MAX_LINES; i++ )
	{
		if ( ! *g_szLineBuffer[i] )
			break;
	}
	if ( i == MAX_LINES )
	{
		// force scroll buffer up
		ScrollTextUp();
		i = MAX_LINES - 1;
	}

	g_iNameLengths[i] = 0;
	g_iNameStarts[i] = 0;
	g_pflNameColors[i] = NULL;
	g_bNoConsole[i] = clientIndex <= 0 && SboChat_IsTransientMessage( pszBuf );	// chat-vgui2: hit messages

	// if it's a say message, search for the players name in the string
	// chat-vgui2: the name is colored for "\x02<name>: " and for the old public form "\x01<name>: "
	if ( ( *pszBuf == 2 || *pszBuf == 1 ) && clientIndex > 0 && clientIndex <= MAX_PLAYERS )	// the server's byte indexes g_PlayerInfoList
	{
		GetPlayerInfo( clientIndex, &g_PlayerInfoList[clientIndex] );
		const char *pName = g_PlayerInfoList[clientIndex].name;

		// SBO 2.0: as it is in the line (without color codes)
		char szName[MAX_PLAYER_NAME_LENGTH + 32];
		if ( pName )
		{
			strncpy( szName, VGui2_Chat_RemoveColorCodes( pName ), sizeof( szName ) - 1 );
			szName[sizeof( szName ) - 1] = 0;
			pName = szName;
		}

		if ( pName )
		{
			// chat-vgui2: after the server's category tag ("\x02*DEAD* <name>: "), else anywhere
			int iTag = SboChat_TagLength( pszBuf + 1 );
			const char *nameInString = iTag ? strstr( pszBuf + 1 + iTag, pName ) : NULL;
			if ( !nameInString )
				nameInString = strstr( pszBuf, pName );

			if ( nameInString )
			{
				g_iNameStarts[i] = ( iTag && nameInString >= pszBuf + 1 + iTag ) ? 1 : nameInString - pszBuf;	// tag coloured with the name
				g_iNameLengths[i] = strlen( pName ) + (nameInString - pszBuf);
				g_pflNameColors[i] = GetClientColor( clientIndex );
			}
		}
	}

	// halflife-updated (halflife #3095): never copy more than the line buffer holds (was max() instead of min())
	strncpy( g_szLineBuffer[i], pszBuf, MAX_CHARS_PER_LINE - 1 );
	g_szLineBuffer[i][MAX_CHARS_PER_LINE - 1] = 0;

	if ( g_pflNameColors[i] )
		g_szLineBuffer[i][0] = 2;	// Draw() colors the name only for lines starting with \x02

	// make sure the text fits in one line
	EnsureTextFitsInOneLineAndWrapIfHaveTo( i );

	// Set scroll time
	if ( i == 0 )
	{
		flScrollTime = gHUD.m_flTime + m_HUD_saytext_time->value;
	}

	m_iFlags |= HUD_ACTIVE;
	PlaySound( "misc/talk.wav", 1 );

	// HUD scaling: HUD units
	if ( gHUD.GetHeight() >= 480 )
		Y_START = gHUD.GetHeight() - 60;
	else
		Y_START = gHUD.GetHeight() - 45;
	Y_START -= (line_height * (MAX_LINES+1));

}

void CHudSayText :: EnsureTextFitsInOneLineAndWrapIfHaveTo( int line )
{
	int line_width = 0;
	GetConsoleStringSize( g_szLineBuffer[line], &line_width, &line_height );

	if ( (line_width + LINE_START) > MAX_LINE_WIDTH )
	{ // string is too long to fit on line
		// scan the string until we find what word is too long,  and wrap the end of the sentence after the word
		int length = LINE_START;
		int tmp_len = 0;
		char *last_break = NULL;
		for ( char *x = g_szLineBuffer[line]; *x != 0; x++ )
		{
			// check for a color change, if so skip past it
			if ( x[0] == '/' && x[1] == '(' )
			{
				x += 2;
				// skip forward until past mode specifier
				while ( *x != 0 && *x != ')' )
					x++;

				if ( *x != 0 )
					x++;

				if ( *x == 0 )
					break;
			}

			char buf[2];
			buf[1] = 0;

			if ( *x == ' ' && x != g_szLineBuffer[line] )  // store each line break,  except for the very first character
				last_break = x;

			buf[0] = *x;  // get the length of the current character
			GetConsoleStringSize( buf, &tmp_len, &line_height );
			length += tmp_len;

			if ( length > MAX_LINE_WIDTH )
			{  // needs to be broken up
				if ( !last_break )
					last_break = x-1;

				x = last_break;

				// find an empty string slot
				int j;
				do 
				{
					for ( j = 0; j < MAX_LINES; j++ )
					{
						if ( ! *g_szLineBuffer[j] )
							break;
					}
					if ( j == MAX_LINES )
					{
						// need to make more room to display text, scroll stuff up then fix the pointers
						int linesmoved = ScrollTextUp();
						line -= linesmoved;
						last_break = last_break - (sizeof(g_szLineBuffer[0]) * linesmoved);
					}
				}
				while ( j == MAX_LINES );

				g_bNoConsole[j] = g_bNoConsole[line];	// chat-vgui2: the wrapped part of a hit message too

				// copy remaining string into next buffer,  making sure it starts with a space character
				if ( (char)*last_break == (char)' ' )
				{
					int linelen = strlen(g_szLineBuffer[j]);
					int remaininglen = strlen(last_break);

					if ( (linelen - remaininglen) <= MAX_CHARS_PER_LINE )
						strcat( g_szLineBuffer[j], last_break );
				}
				else
				{
					if ( (strlen(g_szLineBuffer[j]) - strlen(last_break) - 2) < MAX_CHARS_PER_LINE )
					{
						strcat( g_szLineBuffer[j], " " );
						strcat( g_szLineBuffer[j], last_break );
					}
				}

				*last_break = 0; // cut off the last string

				EnsureTextFitsInOneLineAndWrapIfHaveTo( j );
				break;
			}
		}
	}
}

// SBO: true if the message starts with "*DEAD*"
bool CHudSayText :: IsDead( const char *pszBuf )
{
	if ( strstr( pszBuf, "*DEAD*" ) )
	{
		int iDiff = StringLength( pszBuf ) - StringLength( strstr( pszBuf, "*DEAD*" ) );
		if ( iDiff == 0 )
			return true;
		else
			return false;
	}

	return false;
}

// SBO: strlen + 1 (the terminator is counted); c starts uninitialized. Name fits SBO's thunk slot (likely original).
int StringLength( const char *psz )
{
	char c = 1;	// fix: c was uninitialized (returned 0 when the stack byte happened to be 0)
	int i;

	for ( i = 0; c; i++ )
		c = psz[i];

	return i;
}
