//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// in_win.c -- windows 95 mouse and joystick code
// 02/21/97 JCB Added extended DirectInput code to support external controllers.

#include <math.h>	// HL25 IN_ScaleMouse (before cl_util.h, which defines a fabs macro)
#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "../engine/keydefs.h"
#include "view.h"
#ifdef _WIN32
#include "windows.h"
#else
// Linux: SDL2 is linked (the SDK's inputw32.cpp); <windows.h> types come from sbo_compat.h
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_gamecontroller.h>
#endif
#include "vgui2/vgui2_api.h"
#define VGUI2_CURSOR_VISIBLE() VGui2_IsCursorVisible()	// chat input open: no mouse look

void IN_ResetMouse( void );

#define MOUSE_BUTTON_COUNT 5

// Set this to 1 to show mouse cursor.  Experimental
int	g_iVisibleMouse = 0;

extern "C" 
{
	void DLLEXPORT IN_ActivateMouse( void );
	void DLLEXPORT IN_DeactivateMouse( void );
	void DLLEXPORT IN_MouseEvent (int mstate);
	void DLLEXPORT IN_Accumulate (void);
	void DLLEXPORT IN_ClearStates (void);
}

extern cl_enginefunc_t gEngfuncs;

extern int iMouseInUse;

extern kbutton_t	in_strafe;
extern kbutton_t	in_mlook;
extern kbutton_t	in_speed;
extern kbutton_t	in_jlook;

extern cvar_t	*m_pitch;
extern cvar_t	*m_yaw;
extern cvar_t	*m_forward;
extern cvar_t	*m_side;

extern cvar_t *lookstrafe;
extern cvar_t *lookspring;
extern cvar_t *cl_pitchdown;
extern cvar_t *cl_pitchup;
extern cvar_t *cl_yawspeed;
extern cvar_t *cl_sidespeed;
extern cvar_t *cl_forwardspeed;
extern cvar_t *cl_pitchspeed;
extern cvar_t *cl_movespeedkey;

// mouse variables
cvar_t		*m_filter;
cvar_t		*sensitivity;

// HL25: Custom mouse acceleration (0 disable, 1 to enable, 2 enable with separate yaw/pitch rescale)
static cvar_t *m_customaccel;
//Formula: mousesensitivity = ( rawmousedelta^m_customaccel_exponent ) * m_customaccel_scale + sensitivity
// If mode is 2, then x and y sensitivity are scaled by m_pitch and m_yaw respectively.
// Custom mouse acceleration value.
static cvar_t *m_customaccel_scale;
//Max mouse move scale factor, 0 for no limit
static cvar_t *m_customaccel_max;
//Mouse move is raised to this power before being scaled by scale factor
static cvar_t *m_customaccel_exponent;

// HL25: raw mouse input. m_rawinput is the engine's cvar; with it on, the mouse delta comes from SDL2's relative mouse
// mode instead of moving the Windows cursor to the window centre (Get/SetCursorPos, affected by the Windows pointer
// speed). The Steam engine ships and loads SDL2.dll; the SDK links its import library, SBO resolves the three
// functions it needs from the already loaded module instead (no SDL2 headers/import library in the build). Without
// SDL2 (old engines) the client keeps the cursor method.
// Linux: the engine's window is SDL2's and there is no cursor method; the delta always comes from SDL2's relative
// mouse state, whatever m_rawinput says (the SDK's inputw32.cpp: its GetCursorPos paths are _WIN32 only).
static cvar_t *m_rawinput = NULL;

typedef int ( __cdecl *pfnSDL_SetRelativeMouseMode_t )( int enabled );
typedef unsigned int ( __cdecl *pfnSDL_GetRelativeMouseState_t )( int *x, int *y );
typedef void ( __cdecl *pfnSDL_PumpEvents_t )( void );
typedef int ( __cdecl *pfnSDL_GetRelativeMouseMode_t )( void );

static pfnSDL_SetRelativeMouseMode_t	pfnSDL_SetRelativeMouseMode = NULL;
static pfnSDL_GetRelativeMouseState_t	pfnSDL_GetRelativeMouseState = NULL;
static pfnSDL_PumpEvents_t				pfnSDL_PumpEvents = NULL;
// SBO: optional (NULL: the cached state is trusted, as before); used to notice relative mode turned off behind our back
static pfnSDL_GetRelativeMouseMode_t	pfnSDL_GetRelativeMouseMode = NULL;

static void IN_LoadSDL2( void )
{
#ifndef _WIN32
	pfnSDL_SetRelativeMouseMode = (pfnSDL_SetRelativeMouseMode_t)SDL_SetRelativeMouseMode;
	pfnSDL_GetRelativeMouseState = (pfnSDL_GetRelativeMouseState_t)SDL_GetRelativeMouseState;
	pfnSDL_PumpEvents = SDL_PumpEvents;
	pfnSDL_GetRelativeMouseMode = (pfnSDL_GetRelativeMouseMode_t)SDL_GetRelativeMouseMode;
#else
	HMODULE hSDL2 = GetModuleHandleA( "SDL2.dll" );
	if ( !hSDL2 )
		return;

	pfnSDL_SetRelativeMouseMode = (pfnSDL_SetRelativeMouseMode_t)GetProcAddress( hSDL2, "SDL_SetRelativeMouseMode" );
	pfnSDL_GetRelativeMouseState = (pfnSDL_GetRelativeMouseState_t)GetProcAddress( hSDL2, "SDL_GetRelativeMouseState" );
	pfnSDL_PumpEvents = (pfnSDL_PumpEvents_t)GetProcAddress( hSDL2, "SDL_PumpEvents" );
	pfnSDL_GetRelativeMouseMode = (pfnSDL_GetRelativeMouseMode_t)GetProcAddress( hSDL2, "SDL_GetRelativeMouseMode" );

	if ( !pfnSDL_SetRelativeMouseMode || !pfnSDL_GetRelativeMouseState || !pfnSDL_PumpEvents )
	{
		pfnSDL_SetRelativeMouseMode = NULL;
		pfnSDL_GetRelativeMouseState = NULL;
		pfnSDL_PumpEvents = NULL;
		pfnSDL_GetRelativeMouseMode = NULL;
	}
#endif
}

static bool IN_UseRawInput( void )
{
#ifdef _WIN32
	return m_rawinput && m_rawinput->value != 0 && pfnSDL_GetRelativeMouseState != NULL;
#else
	return true;
#endif
}

static bool mouseRelative = true;

static void IN_SetMouseRelative( bool enable )
{
	// SBO: a refused enable (SDL returns -1, e.g. while the window has no focus) is not cached as on, so the next
	// IN_MouseMove tries again instead of reading zero deltas until the next alt-tab
	int result = 0;
	if ( pfnSDL_SetRelativeMouseMode )
		result = pfnSDL_SetRelativeMouseMode( enable ? 1 : 0 );
	mouseRelative = enable && result == 0;
}

// SBO: the cache says relative mode is on, but SDL says it is off: the engine (level load, reconnect, its own cursor)
// or SDL (focus change) turned it off without telling the client. With the cache alone the view stayed frozen until
// the next alt-tab (IN_ActivateMouse). Same idea as BugfixedHL's IN_SteamOverlayHidden (relative mode set again when
// something else reset it), checked every move instead of on one event.
static bool IN_MouseRelativeLost( void )
{
	return mouseRelative && pfnSDL_GetRelativeMouseMode && !pfnSDL_GetRelativeMouseMode();
}

int			mouse_buttons;
int			mouse_oldbuttonstate;
POINT		current_pos;
int			old_mouse_x, old_mouse_y, mx_accum, my_accum;
float		mouse_x, mouse_y;	// HL25: float, the sensitivity-scaled movement was truncated to whole units

static int	restore_spi;
static int	originalmouseparms[3], newmouseparms[3] = {0, 0, 1};
static int	mouseactive;
int			mouseinitialized;
static int	mouseparmsvalid;
static int	mouseshowtoggle = 1;

// joystick defines and variables
// where should defines be moved?
#define JOY_ABSOLUTE_AXIS	0x00000000		// control like a joystick
#define JOY_RELATIVE_AXIS	0x00000010		// control like a mouse, spinner, trackball
#define	JOY_MAX_AXES		6				// X, Y, Z, R, U, V
#define JOY_AXIS_X			0
#define JOY_AXIS_Y			1
#define JOY_AXIS_Z			2
#define JOY_AXIS_R			3
#define JOY_AXIS_U			4
#define JOY_AXIS_V			5

enum _ControlList
{
	AxisNada = 0,
	AxisForward,
	AxisLook,
	AxisSide,
	AxisTurn
};

// Windows: WinMM joystick, as in SBO 1.38. Linux (docs/dev/LINUX_CLIENT.md): the SDK's SDL GameController code (HL25
// inputw32.cpp, which uses it on every platform) in the _WIN32 #else branches below; the cvars, the axis mapping
// and IN_JoyMove are shared.
#ifdef _WIN32
DWORD dwAxisFlags[JOY_MAX_AXES] =
{
	JOY_RETURNX,
	JOY_RETURNY,
	JOY_RETURNZ,
	JOY_RETURNR,
	JOY_RETURNU,
	JOY_RETURNV
};

#endif

DWORD	dwAxisMap[ JOY_MAX_AXES ];
DWORD	dwControlMap[ JOY_MAX_AXES ];
#ifdef _WIN32
PDWORD	pdwRawValue[ JOY_MAX_AXES ];
#else
int		pdwRawValue[ JOY_MAX_AXES ];
#endif

// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
cvar_t	*in_joystick;
cvar_t	*joy_name;
cvar_t	*joy_advanced;
cvar_t	*joy_advaxisx;
cvar_t	*joy_advaxisy;
cvar_t	*joy_advaxisz;
cvar_t	*joy_advaxisr;
cvar_t	*joy_advaxisu;
cvar_t	*joy_advaxisv;
cvar_t	*joy_forwardthreshold;
cvar_t	*joy_sidethreshold;
cvar_t	*joy_pitchthreshold;
cvar_t	*joy_yawthreshold;
cvar_t	*joy_forwardsensitivity;
cvar_t	*joy_sidesensitivity;
cvar_t	*joy_pitchsensitivity;
cvar_t	*joy_yawsensitivity;
cvar_t	*joy_wwhack1;
cvar_t	*joy_wwhack2;

int			joy_avail, joy_advancedinit, joy_haspov;
DWORD		joy_oldbuttonstate, joy_oldpovstate;

int			joy_id;
DWORD		joy_flags;
DWORD		joy_numbuttons;

#ifdef _WIN32
static JOYINFOEX	ji;
#else
SDL_GameController *s_pJoystick = NULL;
#endif

/*
===========
Force_CenterView_f
===========
*/
void Force_CenterView_f (void)
{
	vec3_t viewangles;

	if (!iMouseInUse)
	{
		gEngfuncs.GetViewAngles( (float *)viewangles );
	    viewangles[PITCH] = 0;
		gEngfuncs.SetViewAngles( (float *)viewangles );
	}
}

/*
===========
IN_ActivateMouse
===========
*/
void DLLEXPORT IN_ActivateMouse (void)
{
	if (mouseinitialized)
	{
#ifdef _WIN32
		if (mouseparmsvalid)
			restore_spi = SystemParametersInfo (SPI_SETMOUSE, 0, newmouseparms, 0);
#endif
		mouseactive = 1;
	}

	// HL25: relative (raw) mouse mode only while no cursor is shown
	if ( g_iVisibleMouse || VGUI2_CURSOR_VISIBLE() || !IN_UseRawInput() )
		IN_SetMouseRelative( false );
	else
		IN_SetMouseRelative( true );

	// halflife-updated: Clear out accumulated mouse input from main menu movement.
	IN_ResetMouse();
}

/*
===========
IN_DeactivateMouse
===========
*/
void DLLEXPORT IN_DeactivateMouse (void)
{
	if (mouseinitialized)
	{
#ifdef _WIN32
		if (restore_spi)
			SystemParametersInfo (SPI_SETMOUSE, 0, originalmouseparms, 0);
#endif

		mouseactive = 0;
	}

	IN_SetMouseRelative( false );	// HL25
}

/*
===========
IN_StartupMouse
===========
*/
void IN_StartupMouse (void)
{
	if ( gEngfuncs.CheckParm ("-nomouse", NULL ) ) 
		return; 

	mouseinitialized = 1;
#ifdef _WIN32
	mouseparmsvalid = SystemParametersInfo (SPI_GETMOUSE, 0, originalmouseparms, 0);

	if (mouseparmsvalid)
	{
		if ( gEngfuncs.CheckParm ("-noforcemspd", NULL ) ) 
			newmouseparms[2] = originalmouseparms[2];

		if ( gEngfuncs.CheckParm ("-noforcemaccel", NULL ) ) 
		{
			newmouseparms[0] = originalmouseparms[0];
			newmouseparms[1] = originalmouseparms[1];
		}

		if ( gEngfuncs.CheckParm ("-noforcemparms", NULL ) ) 
		{
			newmouseparms[0] = originalmouseparms[0];
			newmouseparms[1] = originalmouseparms[1];
			newmouseparms[2] = originalmouseparms[2];
		}
	}
#endif

	mouse_buttons = MOUSE_BUTTON_COUNT;
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown (void)
{
	IN_DeactivateMouse ();
}

/*
===========
IN_GetMousePos

Ask for mouse position from engine
===========
*/
void IN_GetMousePos( int *mx, int *my )
{
	gEngfuncs.GetMousePosition( mx, my );
}

/*
===========
IN_ResetMouse

FIXME: Call through to engine?
===========
*/
void IN_ResetMouse( void )
{
	// HL25/halflife-updated: nothing to do with raw input (SDL keeps the cursor), and don't grab the cursor while
	// the mouse isn't active
#ifdef _WIN32
	if ( !IN_UseRawInput() && mouseactive )
	{
		SetCursorPos ( gEngfuncs.GetWindowCenterX(), gEngfuncs.GetWindowCenterY() );
	}
#endif
}

/*
===========
IN_ResetRelativeMouseState

HL25: drop the raw mouse movement accumulated while a cursor was shown
===========
*/
void IN_ResetRelativeMouseState( void )
{
	if ( IN_UseRawInput() )
	{
		pfnSDL_PumpEvents();
		int deltaX, deltaY;
		pfnSDL_GetRelativeMouseState( &deltaX, &deltaY );
	}
}

/*
===========
IN_ReleaseRelativeMouse

HL25: a VGUI2 panel is about to show the cursor and place it (scoreboard, chat). IN_MouseMove leaves SDL's relative
mouse mode on its next call; do it now, so the position set by the panel moves the real cursor at once. (SDL does not
move the cursor while the relative mode is on.) IN_MouseMove turns the mode back on when no cursor is shown.
===========
*/
void IN_ReleaseRelativeMouse( void )
{
	if ( mouseRelative )
		IN_SetMouseRelative( false );
}

/*
===========
IN_GetMouseSensitivity

HL25: Get mouse sensitivity with sanitization
===========
*/
float IN_GetMouseSensitivity( void )
{
	// Absurdly high sensitivity values can cause the game to hang, so clamp
	if ( sensitivity->value > 10000.0 )
	{
		gEngfuncs.Cvar_SetValue( "sensitivity", 10000.0 );
	}
	else if ( sensitivity->value < 0.01 )
	{
		gEngfuncs.Cvar_SetValue( "sensitivity", 0.01 );
	}

	return sensitivity->value;
}

/*
===========
IN_MouseEvent
===========
*/
void DLLEXPORT IN_MouseEvent (int mstate)
{
	int		i;

	if ( iMouseInUse || g_iVisibleMouse || VGUI2_CURSOR_VISIBLE() )
	{
		// chat-vgui2 (BHL): release buttons that were held when the cursor appeared (no stuck +attack)
		for ( i = 0; i < mouse_buttons; i++ )
		{
			if ( !(mstate & (1<<i)) && (mouse_oldbuttonstate & (1<<i)) )
			{
				gEngfuncs.Key_Event( K_MOUSE1 + i, 0 );
				mouse_oldbuttonstate &= ~(1<<i);
			}
		}
		return;
	}

	// perform button actions
	for (i=0 ; i<mouse_buttons ; i++)
	{
		if ( (mstate & (1<<i)) &&
			!(mouse_oldbuttonstate & (1<<i)) )
		{
			gEngfuncs.Key_Event (K_MOUSE1 + i, 1);
		}

		if ( !(mstate & (1<<i)) &&
			(mouse_oldbuttonstate & (1<<i)) )
		{
			gEngfuncs.Key_Event (K_MOUSE1 + i, 0);
		}
	}	
	
	mouse_oldbuttonstate = mstate;
}

//-----------------------------------------------------------------------------
// Purpose: Allows modulation of mouse scaling/senstivity value and application
//  of custom algorithms.  (HL25)
// Input  : *x -
//			*y -
//-----------------------------------------------------------------------------
void IN_ScaleMouse( float *x, float *y )
{
	float mx = *x;
	float my = *y;

	// This is the default sensitivity
	float mouse_senstivity = ( gHUD.GetSensitivity() != 0 ) ? gHUD.GetSensitivity() : IN_GetMouseSensitivity();

	// Using special accleration values
	if ( m_customaccel->value != 0 )
	{
		float raw_mouse_movement_distance = sqrt( mx * mx + my * my );
		float acceleration_scale = m_customaccel_scale->value;
		float accelerated_sensitivity_max = m_customaccel_max->value;
		float accelerated_sensitivity_exponent = m_customaccel_exponent->value;
		float accelerated_sensitivity = ( (float)pow( raw_mouse_movement_distance, accelerated_sensitivity_exponent ) * acceleration_scale + mouse_senstivity );

		if ( accelerated_sensitivity_max > 0.0001f &&
			accelerated_sensitivity > accelerated_sensitivity_max )
		{
			accelerated_sensitivity = accelerated_sensitivity_max;
		}

		*x *= accelerated_sensitivity;
		*y *= accelerated_sensitivity;

		// Further re-scale by yaw and pitch magnitude if user requests alternate mode 2
		// This means that they will need to up their value for m_customaccel_scale greatly (>40x) since m_pitch/yaw default
		//  to 0.022
		if ( m_customaccel->value == 2 )
		{
			*x *= m_yaw->value;
			*y *= m_pitch->value;
		}
	}
	else
	{
		// Just apply the default
		*x *= mouse_senstivity;
		*y *= mouse_senstivity;
	}
}

/*
===========
IN_MouseMove
===========
*/
void IN_MouseMove ( float frametime, usercmd_t *cmd)
{
	int		mx, my;
	vec3_t viewangles;

	gEngfuncs.GetViewAngles( (float *)viewangles );

	if ( in_mlook.state & 1)
	{
		V_StopPitchDrift ();
	}

	// chat-vgui2: while the VGUI2 cursor (chat input, VGUI1 menus) was visible the OS cursor moved freely. When it
	// hides, re-centre and drop the accumulated movement; otherwise the first frame turns the view by the cursor's
	// distance from the window centre (view "snaps" after sending a chat message).
	{
		static bool s_bCursorWasVisible = false;
		bool bCursorVisible = VGUI2_CURSOR_VISIBLE() || g_iVisibleMouse;
		if ( s_bCursorWasVisible && !bCursorVisible )
		{
			IN_ResetMouse();
			IN_ResetRelativeMouseState();	// HL25: raw input equivalent of the re-centre
			mx_accum = my_accum = 0;
			old_mouse_x = old_mouse_y = 0;
		}
		s_bCursorWasVisible = bCursorVisible;
	}

	//jjb - this disbles normal mouse control if the user is trying to 
	//      move the camera, or if the mouse cursor is visible or if we're in intermission
	if ( !iMouseInUse && !g_iVisibleMouse && !VGUI2_CURSOR_VISIBLE() && !gHUD.m_iIntermission )
	{
		if ( IN_UseRawInput() )
		{
			// HL25: raw input, SDL relative mouse movement since the last call
			int deltaX, deltaY;
			pfnSDL_GetRelativeMouseState( &deltaX, &deltaY );

			mx = deltaX + mx_accum;
			my = deltaY + my_accum;
		}
#ifdef _WIN32
		else
		{
			GetCursorPos (&current_pos);

			mx = current_pos.x - gEngfuncs.GetWindowCenterX() + mx_accum;
			my = current_pos.y - gEngfuncs.GetWindowCenterY() + my_accum;
		}
#endif

		mx_accum = 0;
		my_accum = 0;

		if (m_filter->value)
		{
			mouse_x = (mx + old_mouse_x) * 0.5;
			mouse_y = (my + old_mouse_y) * 0.5;
		}
		else
		{
			mouse_x = mx;
			mouse_y = my;
		}

		old_mouse_x = mx;
		old_mouse_y = my;

		// HL25: Apply custom mouse scaling/acceleration (sensitivity, zoom sensitivity, m_customaccel)
		IN_ScaleMouse( &mouse_x, &mouse_y );

		// add mouse X/Y movement to cmd
		if ( (in_strafe.state & 1) || (lookstrafe->value && (in_mlook.state & 1) ))
			cmd->sidemove += m_side->value * mouse_x;
		else
			viewangles[YAW] -= m_yaw->value * mouse_x;

		if ( (in_mlook.state & 1) && !(in_strafe.state & 1))
		{
			viewangles[PITCH] += m_pitch->value * mouse_y;
			if (viewangles[PITCH] > cl_pitchdown->value)
				viewangles[PITCH] = cl_pitchdown->value;
			if (viewangles[PITCH] < -cl_pitchup->value)
				viewangles[PITCH] = -cl_pitchup->value;
		}
		else
		{
			if ((in_strafe.state & 1) && gEngfuncs.IsNoClipping() )
			{
				cmd->upmove -= m_forward->value * mouse_y;
			}
			else
			{
				cmd->forwardmove -= m_forward->value * mouse_y;
			}
		}

		// if the mouse has moved, force it to the center, so there's room to move
		if ( mx || my )
		{
			IN_ResetMouse();
		}
	}

	gEngfuncs.SetViewAngles( (float *)viewangles );

	// HL25: follow m_rawinput changes and give the cursor back while one is shown (halflife-updated #211)
	if ( ( !IN_UseRawInput() && mouseRelative ) || g_iVisibleMouse || VGUI2_CURSOR_VISIBLE() )
	{
		if ( mouseRelative )
			IN_SetMouseRelative( false );
	}
	else if ( IN_UseRawInput() && ( !mouseRelative || IN_MouseRelativeLost() ) )
	{
		IN_SetMouseRelative( true );
	}

/*
//#define TRACE_TEST
#if defined( TRACE_TEST )
	{
		int mx, my;
		void V_Move( int mx, int my );
		IN_GetMousePos( &mx, &my );
		V_Move( mx, my );
	}
#endif
*/
}

/*
===========
IN_Accumulate
===========
*/
void DLLEXPORT IN_Accumulate (void)
{
	//only accumulate mouse if we are not moving the camera with the mouse
	if ( !iMouseInUse && !g_iVisibleMouse && !VGUI2_CURSOR_VISIBLE() )
	{
	    if (mouseactive)
	    {
			if ( IN_UseRawInput() )
			{
				// HL25: raw input
				int deltaX, deltaY;
				pfnSDL_GetRelativeMouseState( &deltaX, &deltaY );
				mx_accum += deltaX;
				my_accum += deltaY;
			}
#ifdef _WIN32
			else
			{
				GetCursorPos (&current_pos);

				mx_accum += current_pos.x - gEngfuncs.GetWindowCenterX();
				my_accum += current_pos.y - gEngfuncs.GetWindowCenterY();
			}
#endif

			// force the mouse to the center, so there's room to move
			IN_ResetMouse();
		}
	}

}

/*
===================
IN_ClearStates
===================
*/
void DLLEXPORT IN_ClearStates (void)
{
	if ( !mouseactive )
		return;

	mx_accum = 0;
	my_accum = 0;
	mouse_oldbuttonstate = 0;
}

/* 
=============== 
IN_StartupJoystick 
=============== 
*/  
#ifdef _WIN32
void IN_StartupJoystick (void) 
{ 
	int			numdevs;
	JOYCAPS		jc;
	MMRESULT	mmr;
 
 	// assume no joystick
	joy_avail = 0; 

	// abort startup if user requests no joystick
	if ( gEngfuncs.CheckParm ("-nojoy", NULL ) ) 
		return; 
 
	// verify joystick driver is present
	if ((numdevs = joyGetNumDevs ()) == 0)
	{
		gEngfuncs.Con_DPrintf ("joystick not found -- driver not present\n\n");
		return;
	}

	// cycle through the joystick ids for the first valid one
	for (joy_id=0 ; joy_id<numdevs ; joy_id++)
	{
		memset (&ji, 0, sizeof(ji));
		ji.dwSize = sizeof(ji);
		ji.dwFlags = JOY_RETURNCENTERED;

		if ((mmr = joyGetPosEx (joy_id, &ji)) == JOYERR_NOERROR)
			break;
	} 

	// abort startup if we didn't find a valid joystick
	if (mmr != JOYERR_NOERROR)
	{
		gEngfuncs.Con_DPrintf ("joystick not found -- no valid joysticks (%x)\n\n", mmr);
		return;
	}

	// get the capabilities of the selected joystick
	// abort startup if command fails
	memset (&jc, 0, sizeof(jc));
	if ((mmr = joyGetDevCaps (joy_id, &jc, sizeof(jc))) != JOYERR_NOERROR)
	{
		gEngfuncs.Con_DPrintf ("joystick not found -- invalid joystick capabilities (%x)\n\n", mmr); 
		return;
	}

	// save the joystick's number of buttons and POV status
	joy_numbuttons = jc.wNumButtons;
	joy_haspov = jc.wCaps & JOYCAPS_HASPOV;

	// old button and POV states default to no buttons pressed
	joy_oldbuttonstate = joy_oldpovstate = 0;

	// mark the joystick as available and advanced initialization not completed
	// this is needed as cvars are not available during initialization
	gEngfuncs.Con_Printf ("joystick found\n\n", mmr); 
	joy_avail = 1; 
	joy_advancedinit = 0;
}
#else
// HL25 inputw32.cpp: also called from IN_JoyMove (once a second) to find a controller plugged in later
void IN_StartupJoystick (void) 
{ 
	// abort startup if user requests no joystick
	if ( gEngfuncs.CheckParm ("-nojoy", NULL ) ) 
		return; 

	static float flLastCheck = 0.0f;
	if ( flLastCheck > 0.0f && (gEngfuncs.GetAbsoluteTime()-flLastCheck)  < 1.0f )
		return;

	flLastCheck = gEngfuncs.GetAbsoluteTime();

	int nJoysticks = SDL_NumJoysticks();
	if ( nJoysticks > 0 )
	{
		if ( s_pJoystick == NULL )
		{
			for ( int i = 0; i < nJoysticks; i++ )
			{
				if ( SDL_IsGameController( i ) )
				{
					s_pJoystick = SDL_GameControllerOpen( i );
					if ( s_pJoystick )
					{
						//save the joystick's number of buttons and POV status
						joy_numbuttons = SDL_CONTROLLER_BUTTON_MAX;
						joy_haspov = 0;
						
						// old button and POV states default to no buttons pressed
						joy_oldbuttonstate = joy_oldpovstate = 0;
						
						// mark the joystick as available and advanced initialization not completed
						// this is needed as cvars are not available during initialization
						gEngfuncs.Con_Printf ("joystick found %s\n\n", SDL_GameControllerName(s_pJoystick)); 
						joy_avail = 1; 
						joy_advancedinit = 0;
						break;
					}

				}
			}
		}
	}
	else
	{
		if ( s_pJoystick )
			SDL_GameControllerClose( s_pJoystick );
		s_pJoystick = NULL;
		if ( joy_avail )
		{
			joy_avail = 0; 
			gEngfuncs.Con_DPrintf ("joystick not found -- driver not present\n\n");
		}
	}
	
}
#endif


/*
===========
RawValuePointer
===========
*/
#ifdef _WIN32
PDWORD RawValuePointer (int axis)
{
	switch (axis)
	{
	case JOY_AXIS_X:
		return &ji.dwXpos;
	case JOY_AXIS_Y:
		return &ji.dwYpos;
	case JOY_AXIS_Z:
		return &ji.dwZpos;
	case JOY_AXIS_R:
		return &ji.dwRpos;
	case JOY_AXIS_U:
		return &ji.dwUpos;
	case JOY_AXIS_V:
		return &ji.dwVpos;
	}
	// FIX: need to do some kind of error
	return &ji.dwXpos;
}
#else
int RawValuePointer (int axis)
{
	switch (axis)
	{
		default:
		case JOY_AXIS_X:
			return SDL_GameControllerGetAxis( s_pJoystick, SDL_CONTROLLER_AXIS_LEFTX );
		case JOY_AXIS_Y:
			return SDL_GameControllerGetAxis( s_pJoystick, SDL_CONTROLLER_AXIS_LEFTY );
		case JOY_AXIS_Z:
			return SDL_GameControllerGetAxis( s_pJoystick, SDL_CONTROLLER_AXIS_RIGHTX );
		case JOY_AXIS_R:
			return SDL_GameControllerGetAxis( s_pJoystick, SDL_CONTROLLER_AXIS_RIGHTY );
		
	}
}
#endif


/*
===========
Joy_AdvancedUpdate_f
===========
*/
void Joy_AdvancedUpdate_f (void)
{

	// called once by IN_ReadJoystick and by user whenever an update is needed
	// cvars are now available
	int	i;
	DWORD dwTemp;

	// initialize all the maps
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		dwAxisMap[i] = AxisNada;
		dwControlMap[i] = JOY_ABSOLUTE_AXIS;
		pdwRawValue[i] = RawValuePointer(i);
	}

	if( joy_advanced->value == 0.0)
	{
		// default joystick initialization
		// 2 axes only with joystick control
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	}
	else
	{
		if ( strcmp ( joy_name->string, "joystick") != 0 )
		{
			// notify user of advanced controller
			gEngfuncs.Con_Printf ("\n%s configured\n\n", joy_name->string);
		}

		// advanced initialization here
		// data supplied by user via joy_axisn cvars
		dwTemp = (DWORD) joy_advaxisx->value;
		dwAxisMap[JOY_AXIS_X] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_X] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisy->value;
		dwAxisMap[JOY_AXIS_Y] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Y] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisz->value;
		dwAxisMap[JOY_AXIS_Z] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Z] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisr->value;
		dwAxisMap[JOY_AXIS_R] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_R] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisu->value;
		dwAxisMap[JOY_AXIS_U] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_U] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisv->value;
		dwAxisMap[JOY_AXIS_V] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_V] = dwTemp & JOY_RELATIVE_AXIS;
	}

#ifdef _WIN32
	// compute the axes to collect from DirectInput
	joy_flags = JOY_RETURNCENTERED | JOY_RETURNBUTTONS | JOY_RETURNPOV;
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		if (dwAxisMap[i] != AxisNada)
		{
			joy_flags |= dwAxisFlags[i];
		}
	}
#endif
}


/*
===========
IN_Commands
===========
*/
void IN_Commands (void)
{
	int		i, key_index;
	DWORD	buttonstate, povstate;

	if (!joy_avail)
	{
		return;
	}

	
	// loop through the joystick buttons
	// key a joystick event or auxillary event for higher number buttons for each state change
#ifdef _WIN32
	buttonstate = ji.dwButtons;
#else
	buttonstate = 0;
	for ( i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++ )
	{
		if ( SDL_GameControllerGetButton( s_pJoystick, (SDL_GameControllerButton)i ) )
		{
			buttonstate |= 1<<i;
		}
	}
	
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		pdwRawValue[i] = RawValuePointer(i);
	}
#endif
	for (i=0 ; i < (int)joy_numbuttons ; i++)
	{
		if ( (buttonstate & (1<<i)) && !(joy_oldbuttonstate & (1<<i)) )
		{
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			gEngfuncs.Key_Event (key_index + i, 1);
		}

		if ( !(buttonstate & (1<<i)) && (joy_oldbuttonstate & (1<<i)) )
		{
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			gEngfuncs.Key_Event (key_index + i, 0);
		}
	}
	joy_oldbuttonstate = buttonstate;

	if (joy_haspov)
	{
		// convert POV information into 4 bits of state information
		// this avoids any potential problems related to moving from one
		// direction to another without going through the center position
		povstate = 0;
#ifdef _WIN32
		if(ji.dwPOV != JOY_POVCENTERED)
		{
			if (ji.dwPOV == JOY_POVFORWARD)
				povstate |= 0x01;
			if (ji.dwPOV == JOY_POVRIGHT)
				povstate |= 0x02;
			if (ji.dwPOV == JOY_POVBACKWARD)
				povstate |= 0x04;
			if (ji.dwPOV == JOY_POVLEFT)
				povstate |= 0x08;
		}
#endif
		// determine which bits have changed and key an auxillary event for each change
		for (i=0 ; i < 4 ; i++)
		{
			if ( (povstate & (1<<i)) && !(joy_oldpovstate & (1<<i)) )
			{
				gEngfuncs.Key_Event (K_AUX29 + i, 1);
			}

			if ( !(povstate & (1<<i)) && (joy_oldpovstate & (1<<i)) )
			{
				gEngfuncs.Key_Event (K_AUX29 + i, 0);
			}
		}
		joy_oldpovstate = povstate;
	}
}


/* 
=============== 
IN_ReadJoystick
=============== 
*/  
int IN_ReadJoystick (void)
{
#ifndef _WIN32
	SDL_JoystickUpdate();
	return 1;
#else

	memset (&ji, 0, sizeof(ji));
	ji.dwSize = sizeof(ji);
	ji.dwFlags = joy_flags;

	if (joyGetPosEx (joy_id, &ji) == JOYERR_NOERROR)
	{
		// this is a hack -- there is a bug in the Logitech WingMan Warrior DirectInput Driver
		// rather than having 32768 be the zero point, they have the zero point at 32668
		// go figure -- anyway, now we get the full resolution out of the device
		if (joy_wwhack1->value != 0.0)
		{
			ji.dwUpos += 100;
		}
		return 1;
	}
	else
	{
		// read error occurred
		// turning off the joystick seems too harsh for 1 read error,\
		// but what should be done?
		// Con_Printf ("IN_ReadJoystick: no response\n");
		// joy_avail = 0;
		return 0;
	}
#endif
}


/*
===========
IN_JoyMove
===========
*/
void IN_JoyMove ( float frametime, usercmd_t *cmd )
{
	float	speed, aspeed;
	float	fAxisValue, fTemp;
	int		i;
	vec3_t viewangles;

	gEngfuncs.GetViewAngles( (float *)viewangles );


	// complete initialization if first time in
	// this is needed as cvars are not available at initialization time
	if( joy_advancedinit != 1 )
	{
		Joy_AdvancedUpdate_f();
		joy_advancedinit = 1;
	}

#ifndef _WIN32
	// re-scan for joystick presence
	IN_StartupJoystick();
#endif

	// verify joystick is available and that the user wants to use it
	if (!joy_avail || !in_joystick->value)
	{
		return; 
	}
 
	// collect the joystick data, if possible
	if (IN_ReadJoystick () != 1)
	{
		return;
	}

	if (in_speed.state & 1)
		speed = cl_movespeedkey->value;
	else
		speed = 1;

	aspeed = speed * frametime;

	// loop through the axes
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		// get the floating point zero-centered, potentially-inverted data for the current axis
#ifdef _WIN32
		fAxisValue = (float) *pdwRawValue[i];
		// move centerpoint to zero
		fAxisValue -= 32768.0;
#else
		fAxisValue = (float)pdwRawValue[i];
#endif

		if (joy_wwhack2->value != 0.0)
		{
			if (dwAxisMap[i] == AxisTurn)
			{
				// this is a special formula for the Logitech WingMan Warrior
				// y=ax^b; where a = 300 and b = 1.3
				// also x values are in increments of 800 (so this is factored out)
				// then bounds check result to level out excessively high spin rates
				fTemp = 300.0 * pow(fabs(fAxisValue) / 800.0, 1.3);
				if (fTemp > 14000.0)
					fTemp = 14000.0;
				// restore direction information
				fAxisValue = (fAxisValue > 0.0) ? fTemp : -fTemp;
			}
		}

		// convert range from -32768..32767 to -1..1 
		fAxisValue /= 32768.0;

		switch (dwAxisMap[i])
		{
		case AxisForward:
			if ((joy_advanced->value == 0.0) && (in_jlook.state & 1))
			{
				// user wants forward control to become look control
				if (fabs(fAxisValue) > joy_pitchthreshold->value)
				{		
					// if mouse invert is on, invert the joystick pitch value
					// only absolute control support here (joy_advanced is 0)
					if (m_pitch->value < 0.0)
					{
						viewangles[PITCH] -= (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
					else
					{
						viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
					V_StopPitchDrift();
				}
				else
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if(lookspring->value == 0.0)
					{
						V_StopPitchDrift();
					}
				}
			}
			else
			{
				// user wants forward control to be forward control
				if (fabs(fAxisValue) > joy_forwardthreshold->value)
				{
					cmd->forwardmove += (fAxisValue * joy_forwardsensitivity->value) * speed * cl_forwardspeed->value;
				}
			}
			break;

		case AxisSide:
			if (fabs(fAxisValue) > joy_sidethreshold->value)
			{
				cmd->sidemove += (fAxisValue * joy_sidesensitivity->value) * speed * cl_sidespeed->value;
			}
			break;

		case AxisTurn:
			if ((in_strafe.state & 1) || (lookstrafe->value && (in_jlook.state & 1)))
			{
				// user wants turn control to become side control
				if (fabs(fAxisValue) > joy_sidethreshold->value)
				{
					cmd->sidemove -= (fAxisValue * joy_sidesensitivity->value) * speed * cl_sidespeed->value;
				}
			}
			else
			{
				// user wants turn control to be turn control
				if (fabs(fAxisValue) > joy_yawthreshold->value)
				{
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						viewangles[YAW] += (fAxisValue * joy_yawsensitivity->value) * aspeed * cl_yawspeed->value;
					}
					else
					{
						viewangles[YAW] += (fAxisValue * joy_yawsensitivity->value) * speed * 180.0;
					}

				}
			}
			break;

		case AxisLook:
			if (in_jlook.state & 1)
			{
				if (fabs(fAxisValue) > joy_pitchthreshold->value)
				{
					// pitch movement detected and pitch movement desired by user
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
					else
					{
						viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * speed * 180.0;
					}
					V_StopPitchDrift();
				}
				else
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if( lookspring->value == 0.0 )
					{
						V_StopPitchDrift();
					}
				}
			}
			break;

		default:
			break;
		}
	}

	// bounds check pitch
	if (viewangles[PITCH] > cl_pitchdown->value)
		viewangles[PITCH] = cl_pitchdown->value;
	if (viewangles[PITCH] < -cl_pitchup->value)
		viewangles[PITCH] = -cl_pitchup->value;

	gEngfuncs.SetViewAngles( (float *)viewangles );

}

/*
===========
IN_Move
===========
*/
void IN_Move ( float frametime, usercmd_t *cmd)
{
	if ( !iMouseInUse && mouseactive )
	{
		IN_MouseMove ( frametime, cmd);
	}

	IN_JoyMove ( frametime, cmd);
}

/*
===========
IN_Init
===========
*/
void IN_Init (void)
{
	m_filter				= gEngfuncs.pfnRegisterVariable ( "m_filter","0", FCVAR_ARCHIVE );
	sensitivity				= gEngfuncs.pfnRegisterVariable ( "sensitivity","3", FCVAR_ARCHIVE | FCVAR_FILTERSTUFFTEXT ); // user mouse sensitivity setting. HL25/halflife-updated: servers can't stuff it

	in_joystick				= gEngfuncs.pfnRegisterVariable ( "joystick","0", FCVAR_ARCHIVE );
	joy_name				= gEngfuncs.pfnRegisterVariable ( "joyname", "joystick", 0 );
	joy_advanced			= gEngfuncs.pfnRegisterVariable ( "joyadvanced", "0", 0 );
	joy_advaxisx			= gEngfuncs.pfnRegisterVariable ( "joyadvaxisx", "0", 0 );
	joy_advaxisy			= gEngfuncs.pfnRegisterVariable ( "joyadvaxisy", "0", 0 );
	joy_advaxisz			= gEngfuncs.pfnRegisterVariable ( "joyadvaxisz", "0", 0 );
	joy_advaxisr			= gEngfuncs.pfnRegisterVariable ( "joyadvaxisr", "0", 0 );
	joy_advaxisu			= gEngfuncs.pfnRegisterVariable ( "joyadvaxisu", "0", 0 );
	joy_advaxisv			= gEngfuncs.pfnRegisterVariable ( "joyadvaxisv", "0", 0 );
	joy_forwardthreshold	= gEngfuncs.pfnRegisterVariable ( "joyforwardthreshold", "0.15", 0 );
	joy_sidethreshold		= gEngfuncs.pfnRegisterVariable ( "joysidethreshold", "0.15", 0 );
	joy_pitchthreshold		= gEngfuncs.pfnRegisterVariable ( "joypitchthreshold", "0.15", 0 );
	joy_yawthreshold		= gEngfuncs.pfnRegisterVariable ( "joyyawthreshold", "0.15", 0 );
	joy_forwardsensitivity	= gEngfuncs.pfnRegisterVariable ( "joyforwardsensitivity", "-1.0", 0 );
	joy_sidesensitivity		= gEngfuncs.pfnRegisterVariable ( "joysidesensitivity", "-1.0", 0 );
	joy_pitchsensitivity	= gEngfuncs.pfnRegisterVariable ( "joypitchsensitivity", "1.0", 0 );
	joy_yawsensitivity		= gEngfuncs.pfnRegisterVariable ( "joyyawsensitivity", "-1.0", 0 );
	joy_wwhack1				= gEngfuncs.pfnRegisterVariable ( "joywwhack1", "0.0", 0 );
	joy_wwhack2				= gEngfuncs.pfnRegisterVariable ( "joywwhack2", "0.0", 0 );

	// HL25
	m_customaccel			= gEngfuncs.pfnRegisterVariable ( "m_customaccel", "0", FCVAR_ARCHIVE );
	m_customaccel_scale		= gEngfuncs.pfnRegisterVariable ( "m_customaccel_scale", "0.04", FCVAR_ARCHIVE );
	m_customaccel_max		= gEngfuncs.pfnRegisterVariable ( "m_customaccel_max", "0", FCVAR_ARCHIVE );
	m_customaccel_exponent	= gEngfuncs.pfnRegisterVariable ( "m_customaccel_exponent", "1", FCVAR_ARCHIVE );

	m_rawinput				= gEngfuncs.pfnGetCvarPointer ( "m_rawinput" );	// engine cvar (NULL on old engines)
	IN_LoadSDL2();

	gEngfuncs.pfnAddCommand ("force_centerview", Force_CenterView_f);
	gEngfuncs.pfnAddCommand ("joyadvancedupdate", Joy_AdvancedUpdate_f);

	IN_StartupMouse ();
	IN_StartupJoystick ();
}