//=========================================================
// SBO 2.0 color_correction / color_correction_volume, client side: see colorcorrection.h, colorcorrection_core.h,
// game_shared/colorcorrection_shared.h and docs/COLOR_CORRECTION.md. Source SDK 2013's color correction
// (C_ColorCorrection, C_ColorCorrectionVolume, viewpostprocess.cpp) for GoldSrc.
//
// The server sends each entity's state; the client works out every weight itself, from the origin of the view being
// drawn, so spectators, the picture-in-picture view and demos follow their own camera:
//   * color_correction: its weight fades toward the maximum weight (on) or 0 (off) over its fade in / fade out time,
//     times the falloff by the view's distance from it;
//   * color_correction_volume: its weight fades toward the maximum while the view is inside its brush (hull 0, the
//     brush's real shape, not its box), toward 0 outside, over its fade time; the main view and the inset each have
//     their own fade.
// The four heaviest are mixed (colorcorrection_core.cpp Blend) and the view is graded once, right after the view model
// is drawn: the HUD, the crosshair, the screen fades, env_overlay and the VGUI2 panels come after it and keep their
// colors. A view without a view model is graded at the start of the next view, or at the start of the HUD.
// Never the env_sky sky room (it is part of its view), never the spectator's overview map.
//=========================================================

#include <stdio.h>
#include <string.h>
#include <vector>	// before cl_util.h (its fabs macro breaks the standard headers)

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "pm_defs.h"
#include "colorcorrection.h"
#include "colorcorrection_core.h"
#include "colorcorrection_gl.h"

extern "C" playermove_t *pmove;	// pm_shared.c (the client's player move: the engine's hull functions)

namespace
{
const int VIEW_SLOTS = 2;	// the main view, the inset

struct Entry
{
	bool				used;
	colorcorr::Params	p;
	float				received;		// client time of the message (the point entity's fade starts there)
	float				volumeWeight[ VIEW_SLOTS ];	// a volume's fade, per view
	float				volumeTime[ VIEW_SLOTS ];	// when it was last worked out (< 0: not yet: no fade)
	bool				tableDirty;		// the table needs building (new keys or file)
	unsigned int		tex;			// its 3D texture
	int					texSize;
	int					texGeneration;	// the GL context's generation it was made in
	bool				fileReported;	// a bad file is reported once
};

Entry s_Entries[ COLORCORR_MAX_ENTITIES ];
int s_iUsed = 0;	// entries in use (nothing is done per view without any)

cvar_t *s_pCvar = NULL;	// mat_colorcorrection
bool s_bReportedGL = false;

// developer: the first 3D frame after a new map is reported (how many entities the client had, whether it was graded)
bool s_bAwaitFirstView = false;
bool s_bViewThisFrame = false;
bool s_bGradedThisFrame = false;

struct View
{
	bool	set;		// SetView ran for it
	bool	overview;
	int		slot;
	float	origin[3];
	bool	drawn;		// its world is drawn
	bool	graded;
	int		viewport[4];
	bool	haveViewport;
};
View s_View;

// The in-game editor's look (ColorCorr_SetEditorLook): one more table at the editor's weight
struct EditorLook
{
	bool				active = false;
	bool				entities = true;	// the map's entities count too
	float				weight = 0.0f;
	colorcorr::Lut		lut;
	bool				dirty = false;
	unsigned int		tex = 0;
	int					texSize = 0;
	int					texGeneration = 0;
};
EditorLook s_Editor;

// The editor's copy of the view before grading (ColorCorr_RequestSnapshot)
bool s_bSnapshotWanted = false;
std::vector<unsigned char> s_SnapshotRaw;	// as read: bottom row first
std::vector<unsigned char> s_Snapshot;		// top row first
int s_iSnapshotW = 0, s_iSnapshotH = 0, s_iSnapshotSerial = 0;

// Anything to do for a view: entities, the editor's look or a copy wanted
bool Active()
{
	return s_iUsed || s_Editor.active || s_bSnapshotWanted;
}

bool Developer()
{
	return gEngfuncs.pfnGetCvarFloat( "developer" ) > 0.0f;
}

void FreeTexture( Entry &e )
{
	if ( e.tex && e.texGeneration == ColorCorrGL_Generation() )
		ColorCorrGL_DeleteLut( e.tex );
	e.tex = 0;
	e.texSize = 0;
}

void Clear( Entry &e )
{
	FreeTexture( e );
	e = Entry();
}

void ClearAll()
{
	for ( Entry &e : s_Entries )
		if ( e.used )
			Clear( e );
	s_iUsed = 0;
}

Entry *Find( int index, bool create, bool *pCreated = NULL )
{
	if ( pCreated )
		*pCreated = false;
	Entry *pFree = NULL;
	for ( Entry &e : s_Entries )
	{
		if ( e.used && e.p.index == index )
			return &e;
		if ( !e.used && !pFree )
			pFree = &e;
	}
	if ( !create || !pFree )
		return NULL;
	*pFree = Entry();
	pFree->used = true;
	pFree->p.index = index;
	for ( int i = 0; i < VIEW_SLOTS; i++ )
		pFree->volumeTime[i] = -1.0f;
	pFree->tableDirty = true;
	s_iUsed++;
	if ( pCreated )
		*pCreated = true;
	return pFree;
}

bool SameTable( const colorcorr::Params &a, const colorcorr::Params &b )
{
	return !strcmp( a.file, b.file ) && a.saturation == b.saturation && a.contrast == b.contrast
		&& a.brightness == b.brightness && a.gamma == b.gamma && a.tint[0] == b.tint[0] && a.tint[1] == b.tint[1]
		&& a.tint[2] == b.tint[2] && a.tintAmount == b.tintAmount;
}

int MsgFunc_ColorCorr( const char *pszName, int iSize, void *pbuf )
{
	colorcorr::Params p;
	if ( !colorcorr::Parse( (const unsigned char *)pbuf, iSize, p ) )
		return 1;

	if ( p.flags & COLORCORR_FLAG_FIRST )
		ClearAll();

	if ( p.flags & COLORCORR_FLAG_REMOVED )
	{
		Entry *e = Find( p.index, false );
		if ( e )
		{
			Clear( *e );
			s_iUsed--;
		}
		return 1;
	}

	bool isNew = false;
	Entry *e = Find( p.index, true, &isNew );
	if ( !e )
	{
		gEngfuncs.Con_DPrintf( "color_correction: more than %d entities, #%d left out\n", COLORCORR_MAX_ENTITIES, p.index );
		return 1;
	}

	if ( isNew || !SameTable( e->p, p ) || ( e->p.flags & COLORCORR_FLAG_VOLUME ) != ( p.flags & COLORCORR_FLAG_VOLUME ) )
	{
		e->tableDirty = true;
		e->fileReported = false;
	}
	if ( isNew || ( e->p.flags & COLORCORR_FLAG_VOLUME ) != ( p.flags & COLORCORR_FLAG_VOLUME ) )
		for ( int i = 0; i < VIEW_SLOTS; i++ )
			e->volumeTime[i] = -1.0f;
	e->p = p;
	e->received = gEngfuncs.GetClientTime();

	if ( Developer() )
		gEngfuncs.Con_Printf( "color_correction: #%d %s %s weight %.2f/%.2f fade %.2f/%.2f file \"%s\" at %.2f\n", p.index,
			( p.flags & COLORCORR_FLAG_VOLUME ) ? "volume" : "point", ( p.flags & COLORCORR_FLAG_ENABLED ) ? "on" : "off",
			p.weightNow, p.maxWeight, p.fadeIn, p.fadeOut, p.file, e->received );
	return 1;
}

// The view's origin inside the volume's brush: its box first, then hull 0 of the brush model (the shape itself)
bool InsideBrush( const colorcorr::Params &p, const float *origin )
{
	model_t *pModel = gEngfuncs.hudGetModelByIndex( p.model );
	if ( !pModel || pModel->type != mod_brush || !pmove || !pmove->PM_HullPointContents )
		return false;
	float local[3];
	for ( int i = 0; i < 3; i++ )
	{
		local[i] = origin[i] - p.origin[i];
		if ( local[i] < pModel->mins[i] || local[i] > pModel->maxs[i] )
			return false;
	}
	hull_t *hull = &pModel->hulls[0];
	if ( !hull->clipnodes || !hull->planes )
		return false;
	return pmove->PM_HullPointContents( hull, hull->firstclipnode, local ) != CONTENTS_EMPTY;
}

float EntryWeight( Entry &e, const View &v, float now )
{
	const colorcorr::Params &p = e.p;
	const bool enabled = ( p.flags & COLORCORR_FLAG_ENABLED ) != 0;
	if ( p.flags & COLORCORR_FLAG_VOLUME )
	{
		const float target = ( enabled && InsideBrush( p, v.origin ) ) ? p.maxWeight : 0.0f;
		float &w = e.volumeWeight[ v.slot ];
		float &t = e.volumeTime[ v.slot ];
		if ( t < 0.0f || now < t )
			w = target;
		else
			w = colorcorr::Approach( w, target, colorcorr::FadeRate( p.maxWeight, p.fadeIn ), now - t );
		t = now;
		return w;
	}

	const float target = enabled ? p.maxWeight : 0.0f;
	const float rate = colorcorr::FadeRate( p.maxWeight, enabled ? p.fadeIn : p.fadeOut );
	const float w = colorcorr::Approach( p.weightNow, target, rate, now - e.received );
	return colorcorr::FalloffWeight( p, w, v.origin );
}

// The entry's table as a texture (built again for new keys, a new file or a new GL context)
bool PrepareTable( Entry &e )
{
	if ( e.tex && !e.tableDirty && e.texGeneration == ColorCorrGL_Generation() )
		return true;
	FreeTexture( e );
	e.tableDirty = false;

	colorcorr::Lut file, table;
	bool haveFile = false;
	if ( e.p.file[0] )
	{
		int len = 0;
		byte *data = gEngfuncs.COM_LoadFile( e.p.file, 5, &len );
		char err[160] = "";
		if ( !data )
			snprintf( err, sizeof( err ), "can't open it" );
		else
		{
			haveFile = colorcorr::ReadTable( e.p.file, data, len, file, err, sizeof( err ) );
			gEngfuncs.COM_FreeFile( data );
		}
		if ( !haveFile && !e.fileReported )
		{
			gEngfuncs.Con_DPrintf( "color_correction: \"%s\": %s (the simple keys only)\n", e.p.file, err );
			e.fileReported = true;
		}
	}
	colorcorr::BuildLut( e.p, haveFile ? &file : NULL, table );
	ColorCorrGL_SetDebug( Developer() );
	e.tex = ColorCorrGL_CreateLut( table );
	if ( Developer() && ColorCorrGL_LastTableError() )
		gEngfuncs.Con_Printf( "color_correction: GL error 0x%x making the table of #%d\n", ColorCorrGL_LastTableError(),
			e.p.index );
	e.texSize = table.size;
	e.texGeneration = ColorCorrGL_Generation();
	return e.tex != 0;
}

// The editor's table as a texture (built again for a new table or a new GL context)
bool PrepareEditorTable()
{
	EditorLook &e = s_Editor;
	if ( e.tex && !e.dirty && e.texGeneration == ColorCorrGL_Generation() )
		return true;
	if ( e.tex && e.texGeneration == ColorCorrGL_Generation() )
		ColorCorrGL_DeleteLut( e.tex );
	e.tex = 0;
	e.dirty = false;
	e.tex = ColorCorrGL_CreateLut( e.lut );
	e.texSize = e.lut.size;
	e.texGeneration = ColorCorrGL_Generation();
	return e.tex != 0;
}

// The copy of the view as read (bottom row first) turned top row first
void StoreSnapshot()
{
	const int w = s_iSnapshotW, h = s_iSnapshotH;
	if ( w <= 0 || h <= 0 || s_SnapshotRaw.size() != (size_t)w * h * 3 )
		return;
	s_Snapshot.resize( s_SnapshotRaw.size() );
	const size_t row = (size_t)w * 3;
	for ( int y = 0; y < h; y++ )
		memcpy( &s_Snapshot[ (size_t)y * row ], &s_SnapshotRaw[ (size_t)( h - 1 - y ) * row ], row );
	s_SnapshotRaw.clear();
	s_SnapshotRaw.shrink_to_fit();
	s_iSnapshotSerial++;
}

void Grade( View &v )
{
	v.graded = true;
	if ( !Active() || v.overview || !v.haveViewport )
		return;

	// mat_colorcorrection 0: no grade at all (the editor's copy of the view is still made)
	const bool on = !( s_pCvar && s_pCvar->value == 0.0f );
	const bool snapshot = s_bSnapshotWanted && v.slot == 0;

	const float now = gEngfuncs.GetClientTime();
	float weights[ COLORCORR_MAX_ENTITIES + 1 ];
	bool exclusive[ COLORCORR_MAX_ENTITIES + 1 ];
	Entry *entries[ COLORCORR_MAX_ENTITIES + 1 ];
	int n = 0;
	if ( on && ( !s_Editor.active || s_Editor.entities ) )
		for ( Entry &e : s_Entries )
		{
			if ( !e.used )
				continue;
			entries[n] = &e;
			weights[n] = EntryWeight( e, v, now );
			exclusive[n] = ( e.p.flags & COLORCORR_FLAG_EXCLUSIVE ) != 0;
			n++;
		}
	int editorIndex = -1;	// the editor's look: entries[] holds no Entry for it
	if ( on && s_Editor.active && s_Editor.weight > 0.0f )
	{
		editorIndex = n;
		entries[n] = NULL;
		weights[n] = s_Editor.weight;
		exclusive[n] = false;
		n++;
	}

	int pick[ colorcorr::BLEND_MAX ];
	float pickWeight[ colorcorr::BLEND_MAX ];
	float identity = 1.0f;
	int count = colorcorr::Blend( weights, exclusive, n, pick, pickWeight, identity );
	if ( !count || identity >= 0.999f )
	{
		if ( !snapshot )
			return;
		count = 0;	// the copy only: the pass draws the view unchanged
		identity = 1.0f;
	}

	if ( !ColorCorrGL_Available() )
	{
		if ( !s_bReportedGL )
		{
			gEngfuncs.Con_Printf( "color_correction: not drawn: %s\n", ColorCorrGL_Why() );
			s_bReportedGL = true;
		}
		return;
	}

	unsigned int tex[ colorcorr::BLEND_MAX ];
	int sizes[ colorcorr::BLEND_MAX ];
	float w[ colorcorr::BLEND_MAX ];
	int used = 0;
	float lost = 0.0f;	// the weight of a table that could not be made: the picture unchanged instead
	for ( int i = 0; i < count; i++ )
	{
		if ( pick[i] == editorIndex )
		{
			if ( !PrepareEditorTable() )
			{
				lost += pickWeight[i];
				continue;
			}
			tex[used] = s_Editor.tex;
			sizes[used] = s_Editor.texSize;
			w[used] = pickWeight[i];
			used++;
			continue;
		}
		Entry &e = *entries[ pick[i] ];
		if ( !PrepareTable( e ) )
		{
			lost += pickWeight[i];
			continue;
		}
		tex[used] = e.tex;
		sizes[used] = e.texSize;
		w[used] = pickWeight[i];
		used++;
	}
	if ( !used && !snapshot )
		return;
	const bool debug = Developer();
	ColorCorrGL_SetDebug( debug );
	if ( snapshot )
		ColorCorrGL_SetSnapshot( &s_SnapshotRaw, &s_iSnapshotW, &s_iSnapshotH );
	if ( ColorCorrGL_Apply( v.viewport, identity + lost, used, tex, sizes, w ) && used )
		s_bGradedThisFrame = true;
	if ( snapshot )
	{
		ColorCorrGL_SetSnapshot( NULL, NULL, NULL );
		s_bSnapshotWanted = false;
		StoreSnapshot();
	}
	if ( debug )
	{
		unsigned int engineError = 0;
		const unsigned int error = ColorCorrGL_LastError( &engineError );
		if ( error )
			gEngfuncs.Con_Printf( "color_correction: GL error 0x%x after the pass\n", error );
		if ( engineError )
			gEngfuncs.Con_DPrintf( "color_correction: GL error 0x%x was set before the pass (not ours)\n", engineError );
	}
}
} // namespace

void ColorCorr_Init( void )
{
	gEngfuncs.pfnHookUserMsg( COLORCORR_MSG_NAME, MsgFunc_ColorCorr );
	// Source's name: 0 turns color correction off for this player
	s_pCvar = CVAR_CREATE( "mat_colorcorrection", "1", FCVAR_ARCHIVE );
}

void ColorCorr_Reset( void )
{
	ClearAll();
	s_View = View();
}

void ColorCorr_VidInit( void )
{
	ColorCorr_Reset();
	ColorCorr_SetEditorLook( NULL, 0, 0.0f, true );	// the editor gives it again while it is open
	if ( Developer() )
	{
		int live = 0, made = 0;
		ColorCorrGL_Counts( &live, &made );
		gEngfuncs.Con_Printf( "color_correction: new map: %d tables alive, %d pass objects made\n", live, made );
	}
	s_bReportedGL = false;
	// the frame the new map was loaded in may have drawn (and graded) the last map's view before this: not counted
	s_bAwaitFirstView = true;
	s_bViewThisFrame = false;
	s_bGradedThisFrame = false;
}

void ColorCorr_Flush( void )
{
	if ( s_View.set && s_View.drawn && !s_View.graded )
		Grade( s_View );
}

void ColorCorr_SetView( const float *origin, bool overview, bool inset )
{
	if ( !overview )
		s_bViewThisFrame = true;
	s_View = View();
	s_View.set = true;
	s_View.overview = overview;
	s_View.slot = inset ? 1 : 0;
	s_View.origin[0] = origin[0];
	s_View.origin[1] = origin[1];
	s_View.origin[2] = origin[2];
}

void ColorCorr_ViewDrawn( void )
{
	if ( !s_View.set || s_View.overview || s_View.graded )
		return;
	s_View.drawn = true;
	if ( Active() && ColorCorrGL_Available() )
		s_View.haveViewport = ColorCorrGL_GetViewport( s_View.viewport );
}

void ColorCorr_AfterViewModel( void )
{
	if ( !s_View.set || s_View.overview || s_View.graded )
		return;
	s_View.drawn = true;
	if ( Active() && ColorCorrGL_Available() )
		s_View.haveViewport = ColorCorrGL_GetViewport( s_View.viewport );
	Grade( s_View );
}

void ColorCorr_FrameEnd( void )
{
	ColorCorr_Flush();
	if ( s_bAwaitFirstView && s_bViewThisFrame )
	{
		s_bAwaitFirstView = false;
		if ( Developer() )
			gEngfuncs.Con_Printf( "color_correction: first 3D frame of the map: %d entities, %s (client time %.2f)\n",
				s_iUsed, s_bGradedThisFrame ? "graded" : "not graded", gEngfuncs.GetClientTime() );
	}
	s_bViewThisFrame = false;
	s_bGradedThisFrame = false;
	s_View.set = false;
}

void ColorCorr_SetEditorLook( const unsigned char *rgb, int size, float weight, bool entities )
{
	EditorLook &e = s_Editor;
	if ( !rgb || size < colorcorr::CUBE_MIN_SIZE || size > colorcorr::CUBE_MAX_SIZE )
	{
		e.active = false;
		e.weight = 0.0f;
		e.entities = true;
		return;
	}
	const size_t bytes = (size_t)size * size * size * 3;
	if ( e.lut.size != size || e.lut.rgb.size() != bytes || memcmp( e.lut.rgb.data(), rgb, bytes ) != 0 )
	{
		e.lut.size = size;
		e.lut.rgb.assign( rgb, rgb + bytes );
		e.dirty = true;
	}
	e.active = true;
	e.weight = weight < 0.0f ? 0.0f : ( weight > 1.0f ? 1.0f : weight );
	e.entities = entities;
}

bool ColorCorr_HasEditorLook( void )
{
	return s_Editor.active;
}

void ColorCorr_RequestSnapshot( void )
{
	s_bSnapshotWanted = true;
}

bool ColorCorr_GetSnapshot( const unsigned char **rgb, int *wide, int *tall, int *serial )
{
	if ( serial )
		*serial = s_iSnapshotSerial;
	if ( !s_iSnapshotSerial || s_Snapshot.size() != (size_t)s_iSnapshotW * s_iSnapshotH * 3 )
		return false;
	if ( rgb )
		*rgb = s_Snapshot.data();
	if ( wide )
		*wide = s_iSnapshotW;
	if ( tall )
		*tall = s_iSnapshotH;
	return true;
}
