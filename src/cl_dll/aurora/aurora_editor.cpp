// SBO 2.0: the in-game particle editor's preview. See aurora_editor.h.

#include <math.h>	// before cl_util.h (its fabs, min and max macros)
#include <string.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "event_api.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "pmtrace.h"
#include "pm_defs.h"
#include "particlemgr.h"
#include "particlesys.h"
#include "aurora_editor.h"

extern vec3_t v_origin;							// view.cpp: the origin of the view being drawn
void HUD_GetEffectAngles( float *angles );		// tri.cpp: its angles

namespace
{
const float PLACE_DISTANCE = 128.0f;	// in front of the view
const float PLACE_MAX = 4096.0f;		// a picked point at most this far
const float WALL_GAP = 16.0f;			// off the surface hit

enum { PLACE_NONE, PLACE_FRONT, PLACE_SCREEN };

std::string s_Text;
std::string s_Name = "editor";
bool s_bWant = false;
bool s_bDirty = false;
bool s_bOn = true;
bool s_bPlaced = false;
int s_iPlaceRequest = PLACE_NONE;
float s_flScreenX = 0.5f, s_flScreenY = 0.5f;
Vector s_vecOrigin( 0, 0, 0 );
float s_flPitch = 0, s_flYaw = 0;
std::string s_Level;

std::vector<std::string> s_Problems;

void CollectProblem( const char *line )
{
	if ( s_Problems.size() < 64 )
		s_Problems.push_back( line );
}

// the world along the line from start in dir: the point hit, WALL_GAP in front of it (or the line's end)
Vector TraceOut( const Vector &start, const Vector &dir, float length )
{
	const Vector end = start + dir * length;
	pmtrace_t tr;
	// the world as it is now (ev_hldm.cpp's trace set-up): the player's own physics state is not used
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( -1 );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( (float *)&start, (float *)&end, PM_WORLD_ONLY, -1, &tr );
	gEngfuncs.pEventAPI->EV_PopPMStates();
	if ( tr.startsolid || tr.allsolid )
		return start + dir * ( length < 64.0f ? length : 64.0f );
	if ( tr.fraction >= 1.0f )
		return end;
	float back = WALL_GAP;
	if ( back > length * tr.fraction )
		back = length * tr.fraction;
	return Vector( tr.endpos ) - dir * back;
}

void ResolvePlace( void )
{
	Vector angles, forward, right, up;
	HUD_GetEffectAngles( angles );
	AngleVectors( angles, forward, right, up );
	const Vector view = v_origin;

	if ( s_iPlaceRequest == PLACE_SCREEN )
	{
		// the screen point back into the world (XDM in_camera.cpp TraceClick): x -1 .. 1 across, y 1 .. -1 down
		float screen[3] = { s_flScreenX * 2.0f - 1.0f, 1.0f - s_flScreenY * 2.0f, 0.0f };
		float nearPt[3], farPt[3];
		gEngfuncs.pTriAPI->ScreenToWorld( screen, nearPt );
		screen[2] = 1.0f;
		gEngfuncs.pTriAPI->ScreenToWorld( screen, farPt );
		Vector dir = Vector( farPt ) - Vector( nearPt );
		if ( dir.Length() > 0.001f )
			s_vecOrigin = TraceOut( view, dir.Normalize(), PLACE_MAX );
		else
			s_vecOrigin = TraceOut( view, forward, PLACE_DISTANCE );
	}
	else
	{
		s_vecOrigin = TraceOut( view, forward, PLACE_DISTANCE );
	}
	s_bPlaced = true;
	s_iPlaceRequest = PLACE_NONE;
	gEngfuncs.Con_DPrintf( "particleeditorui: the emitter is at %.0f %.0f %.0f\n", s_vecOrigin.x, s_vecOrigin.y,
	    s_vecOrigin.z );
}

void RemovePreview( void )
{
	ParticleSystem *pSystem = g_ParticleSystems.FindSystem( AURORA_EDITOR_ID );
	if ( pSystem )
		g_ParticleSystems.RemoveSystem( pSystem );
}
} // namespace

void AuroraEditor_SetPreview( const char *text, const char *name )
{
	if ( !text )
	{
		s_bWant = false;
		s_Text.clear();
		RemovePreview();
		return;
	}
	if ( !s_bWant || s_Text != text )
		s_bDirty = true;
	s_bWant = true;
	s_Text = text;
	s_Name = name && name[0] ? name : "editor";
}

void AuroraEditor_SetPreviewOn( bool on )
{
	s_bOn = on;
}

void AuroraEditor_Restart( void )
{
	s_bDirty = true;
}

void AuroraEditor_SetTurn( float pitch, float yaw )
{
	s_flPitch = pitch;
	s_flYaw = yaw;
}

void AuroraEditor_PlaceInFront( void )
{
	s_iPlaceRequest = PLACE_FRONT;
}

void AuroraEditor_PlaceAtScreen( float x, float y )
{
	s_flScreenX = x;
	s_flScreenY = y;
	s_iPlaceRequest = PLACE_SCREEN;
}

int AuroraEditor_Problems( char *first, int firstSize )
{
	if ( first && firstSize > 0 )
	{
		first[0] = 0;
		if ( !s_Problems.empty() )
		{
			strncpy( first, s_Problems[0].c_str(), firstSize - 1 );
			first[firstSize - 1] = 0;
		}
	}
	return (int)s_Problems.size();
}

bool AuroraEditor_LoadSprite( const char *name, char *err, int errSize )
{
	if ( err && errSize > 0 )
		err[0] = 0;
	if ( !name || !name[0] || strstr( name, ".." ) || name[0] == '/' || name[0] == '\\' || strchr( name, ':' ) )
	{
		if ( err )
			snprintf( err, errSize, "the name must be a path inside the game folder" );
		return false;
	}
	int iLength = 0;
	byte *pFile = gEngfuncs.COM_LoadFile( (char *)name, 5, &iLength );
	if ( !pFile )
	{
		if ( err )
			snprintf( err, errSize, "%s isn't in the game's folders", name );
		return false;
	}
	gEngfuncs.COM_FreeFile( pFile );
	if ( !Aurora_LoadSprite( name ) )
	{
		if ( err )
			snprintf( err, errSize, "the game couldn't load %s: it isn't a sprite, or the game has no room for more sprites", name );
		return false;
	}
	return true;
}

int AuroraEditor_PreviewParticles( void )
{
	ParticleSystem *pSystem = g_ParticleSystems.FindSystem( AURORA_EDITOR_ID );
	int n = 0;
	if ( pSystem )
	{
		for ( particle *p = pSystem->m_pActiveParticle; p && n < 100000; p = p->nextpart )
			n++;
	}
	return n;
}

void AuroraEditor_Frame( void )
{
	// a new map: the place belongs to the old one
	const char *level = gEngfuncs.pfnGetLevelName();
	if ( !level )
		level = "";
	if ( s_Level != level )
	{
		s_Level = level;
		s_bPlaced = false;
	}

	if ( !s_bWant )
		return;

	// the spectator's camera inset is drawn with its own view: the place waits for the main view
	if ( !gHUD.m_Spectator.m_bInsetDrawPass && ( s_iPlaceRequest != PLACE_NONE || !s_bPlaced ) )
	{
		if ( !s_bPlaced && s_iPlaceRequest == PLACE_NONE )
			s_iPlaceRequest = PLACE_FRONT;
		ResolvePlace();
	}
	if ( !s_bPlaced )
		return;

	ParticleSystem *pSystem = g_ParticleSystems.FindSystem( AURORA_EDITOR_ID );
	if ( pSystem && s_bDirty )
	{
		g_ParticleSystems.RemoveSystem( pSystem );
		pSystem = NULL;
	}
	if ( !pSystem )
	{
		// every problem of this text, for the editor (the console gets none: the text changes with each edit)
		s_Problems.clear();
		g_pfnAuroraWarnSink = CollectProblem;
		pSystem = new ParticleSystem( AURORA_EDITOR_ID, s_Name.c_str(), s_Text.c_str() );
		g_pfnAuroraWarnSink = NULL;
		g_ParticleSystems.AddSystem( pSystem );
		s_bDirty = false;
	}

	AuroraPlacement placement;
	placement.origin = s_vecOrigin;
	placement.angles = Vector( s_flPitch, s_flYaw, 0.0f );
	placement.iTarget = 0;
	placement.iAttachment = -1;
	pSystem->SetPlacement( placement );
	pSystem->SetOn( s_bOn );
}
