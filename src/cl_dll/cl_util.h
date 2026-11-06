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
// cl_util.h
//

#include "cvardef.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

// Macros to hook function calls into the HUD object
#define HOOK_MESSAGE(x) gEngfuncs.pfnHookUserMsg(#x, __MsgFunc_##x );

#define DECLARE_MESSAGE(y, x) int __MsgFunc_##x(const char *pszName, int iSize, void *pbuf) \
							{ \
							return gHUD.y.MsgFunc_##x(pszName, iSize, pbuf ); \
							}


#define HOOK_COMMAND(x, y) gEngfuncs.pfnAddCommand( x, __CmdFunc_##y );
#define DECLARE_COMMAND(y, x) void __CmdFunc_##x( void ) \
							{ \
								gHUD.y.UserCmd_##x( ); \
							}

inline float CVAR_GET_FLOAT( const char *x ) {	return gEngfuncs.pfnGetCvarFloat( (char*)x ); }
inline char* CVAR_GET_STRING( const char *x ) {	return gEngfuncs.pfnGetCvarString( (char*)x ); }
inline struct cvar_s *CVAR_CREATE( const char *cv, const char *val, const int flags ) {	return gEngfuncs.pfnRegisterVariable( (char*)cv, (char*)val, flags ); }

// halflife-updated PR #245 (HUD scaling via triangle API): SPR_Set/SPR_Draw*/FillRGBA/SetCrosshair and the console
// string functions are functions (hud_scale.cpp) taking HUD units (gHUD.GetWidth() x gHUD.GetHeight()). Without
// hud_scale they call the engine unchanged. Code that draws in screen pixels (VGUI, spectator overview) must call
// gEngfuncs.pfn* directly.
// SBO: SPR_Load is a function (util.cpp): the single place the client loads HUD sprites through, so the
// "hudspritelist" command can also name the loads that failed. The engine holds at most MAX_HUD_SPRITES of them.
#define MAX_HUD_SPRITES			256		// engine limit ("cannot allocate more than %d HUD sprites")
#define HUD_SPRITELIST_COMMAND	"hudspritelist"
HSPRITE SPR_Load( const char *szPicName );
void HudSpriteList_f( void );
void SPR_Set( HSPRITE hPic, int r, int g, int b );
#define SPR_Frames (*gEngfuncs.pfnSPR_Frames)
#define SPR_GetList (*gEngfuncs.pfnSPR_GetList)

// SPR_Draw  draws a the current sprite as solid
void SPR_Draw( int frame, int x, int y, const wrect_t *prc );
// SPR_DrawHoles  draws the current sprites,  with color index255 not drawn (transparent)
void SPR_DrawHoles( int frame, int x, int y, const wrect_t *prc );
// SPR_DrawAdditive  adds the sprites RGB values to the background  (additive transulency)
void SPR_DrawAdditive( int frame, int x, int y, const wrect_t *prc );
// SBO: draws the texels s0,t0 - s1,t1 of the current sprite into the screen rectangle x0,y0 - x1,y1 (pixels; solid,
// or with holes), with or without hud_scale; hardware renderer only (HUD_IsHardware). The sniper scope.
void SPR_DrawTexels( int frame, int x0, int y0, int x1, int y1, float s0, float t0, float s1, float t1, bool bHoles );

// SPR_EnableScissor  sets a clipping rect for HUD sprites.  (0,0) is the top-left hand corner of the screen.
#define SPR_EnableScissor (*gEngfuncs.pfnSPR_EnableScissor)
// SPR_DisableScissor  disables the clipping rect
#define SPR_DisableScissor (*gEngfuncs.pfnSPR_DisableScissor)
//
void FillRGBA( int x, int y, int width, int height, int r, int g, int b, int a );


// ScreenHeight returns the height of the screen, in pixels
#define ScreenHeight (gHUD.m_scrinfo.iHeight)
// ScreenWidth returns the width of the screen, in pixels
#define ScreenWidth (gHUD.m_scrinfo.iWidth)

// Use this to set any co-ords in 640x480 space
#define XRES(x)		((int)(float(x)  * ((float)ScreenWidth / 640.0f) + 0.5f))
#define YRES(y)		((int)(float(y)  * ((float)ScreenHeight / 480.0f) + 0.5f))

// HUD scaling (hud_scale.cpp): the same for HUD elements, in HUD units (gHUD.GetWidth() x gHUD.GetHeight())
#define HUD_XRES(x)	((int)(float(x)  * ((float)gHUD.GetWidth() / 640.0f) + 0.5f))
#define HUD_YRES(y)	((int)(float(y)  * ((float)gHUD.GetHeight() / 480.0f) + 0.5f))

bool HUD_IsHardware( void );	// false on the software renderer: no stretched sprites, no hud_scale

// HUD units <-> screen pixels (identity without hud_scale)
float HUD_GetScale( void );
int HUD_ToScreenX( int x );
int HUD_ToScreenY( int y );
int HUD_ToHudX( int x );
int HUD_ToHudY( int y );
int HUD_ToHudSize( int size );	// a size in pixels (e.g. font height) in HUD units, rounded up

// use this to project world coordinates to screen coordinates
#define XPROJECT(x)	( (1.0f+(x))*ScreenWidth*0.5f )
#define YPROJECT(y) ( (1.0f-(y))*ScreenHeight*0.5f )

#define GetScreenInfo (*gEngfuncs.pfnGetScreenInfo)
#define ServerCmd (*gEngfuncs.pfnServerCmd)
#define ClientCmd (*gEngfuncs.pfnClientCmd)
// HL25: for command strings that come from the server/HLTV (the engine applies its stufftext filter, e.g. FCVAR_FILTERSTUFFTEXT cvars).
// Needs cl_enginefunc_t::pfnFilteredClientCmd (engine/cdll_int.h, HL25 layout) and an HL25 engine.
#define EngineFilteredClientCmd (*gEngfuncs.pfnFilteredClientCmd)
void SetCrosshair( HSPRITE hspr, wrect_t rc, int r, int g, int b );
#define AngleVectors (*gEngfuncs.pfnAngleVectors)


// Gets the height & width of a sprite,  at the specified frame
inline int SPR_Height( HSPRITE x, int f )	{ return gEngfuncs.pfnSPR_Height(x, f); }
inline int SPR_Width( HSPRITE x, int f )	{ return gEngfuncs.pfnSPR_Width(x, f); }

inline 	client_textmessage_t	*TextMessageGet( const char *pName ) { return gEngfuncs.pfnTextMessageGet( pName ); }
inline 	int						TextMessageDrawChar( int x, int y, int number, int r, int g, int b ) 
{ 
	return gEngfuncs.pfnDrawCharacter( x, y, number, r, g, b ); 
}

// HUD units; the text keeps the console font's size in pixels
int DrawConsoleString( int x, int y, const char *string );
void GetConsoleStringSize( const char *string, int *width, int *height );

inline int ConsoleStringLen( const char *string )
{
	int _width, _height;
	GetConsoleStringSize( string, &_width, &_height );
	return _width;
}

inline void ConsolePrint( const char *string )
{
	gEngfuncs.pfnConsolePrint( string );
}

inline void CenterPrint( const char *string )
{
	gEngfuncs.pfnCenterPrint( string );
}

// returns the players name of entity no.
#define GetPlayerInfo (*gEngfuncs.pfnGetPlayerInfo)

// halflife-updated/HL25: bounded string helpers for text received from the server
inline char* safe_strcpy( char *dst, const char *src, int len_dst )
{
	if ( len_dst <= 0 )
		return NULL; // this is bad

	strncpy( dst, src, len_dst );
	dst[len_dst - 1] = '\0';

	return dst;
}

inline int safe_sprintf( char *dst, int len_dst, const char *format, ... )
{
	if ( len_dst <= 0 )
		return -1; // this is bad

	va_list v;
	va_start( v, format );
	vsnprintf( dst, len_dst, format, v );
	va_end( v );

	dst[len_dst - 1] = '\0';

	return 0;
}

// sound functions
inline void PlaySound( char *szSound, float vol ) { gEngfuncs.pfnPlaySoundByName( szSound, vol ); }
inline void PlaySound( int iSound, float vol ) { gEngfuncs.pfnPlaySoundByIndex( iSound, vol ); }

#define max(a, b)  (((a) > (b)) ? (a) : (b))
#define min(a, b)  (((a) < (b)) ? (a) : (b))
#define fabs(x)	   ((x) > 0 ? (x) : 0 - (x))

void ScaleColors( int &r, int &g, int &b, int a );

#define DotProduct(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSubtract(a,b,c) {(c)[0]=(a)[0]-(b)[0];(c)[1]=(a)[1]-(b)[1];(c)[2]=(a)[2]-(b)[2];}
#define VectorAdd(a,b,c) {(c)[0]=(a)[0]+(b)[0];(c)[1]=(a)[1]+(b)[1];(c)[2]=(a)[2]+(b)[2];}
#define VectorCopy(a,b) {(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];}
inline void VectorClear(float *a) { a[0]=0.0;a[1]=0.0;a[2]=0.0;}
float Length(const float *v);
void VectorMA (const float *veca, float scale, const float *vecb, float *vecc);
void VectorScale (const float *in, float scale, float *out);
float VectorNormalize (float *v);
void VectorInverse ( float *v );

extern vec3_t vec3_origin;

// disable 'possible loss of data converting float to int' warning message
#pragma warning( disable: 4244 )
// disable 'truncation from 'const double' to 'float' warning message
#pragma warning( disable: 4305 )

inline void UnpackRGB(int &r, int &g, int &b, unsigned long ulRGB)\
{\
	r = (ulRGB & 0xFF0000) >>16;\
	g = (ulRGB & 0xFF00) >> 8;\
	b = ulRGB & 0xFF;\
}

HSPRITE LoadSprite(const char *pszName);
