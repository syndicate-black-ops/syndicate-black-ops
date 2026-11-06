//========= SBO world-space VGUI2 screens =========//
//
// Purpose: SBO side of the world screens (see worldscreen.h): the WScreen message, the screens' places, aim and
//          clicks, the visibility tests, the world quads in HUD_DrawNormalTriangles, cursor and crosshair.
//
// Commands / cvars:
//   worldscreen_status     the screens, aim, and the GL findings (FBO support, render targets, engine GL state)
//   worldscreen_debug 0    1: state changes, GL errors, engine GL state checks, 2: also every render / draw
//   worldscreen_rate 10    texture updates per second (only while the screen can be seen, see below)
//   worldscreen_update_dist 1024   no texture updates beyond this distance (the quad keeps its last image)
//
// Placement (the server's vgui_screen, SBO Source's rule): the entity's origin is the lower-left corner seen from
// the front, the screen faces along the entity's angles (forward vector), "width" runs to the viewer's right (the
// entity's -right vector), "height" up.
//
// Bound screens (vgui_screen "surface", WSCREEN_FLAG_BOUND): the panel is drawn on the carrier brush entity's
// {screen faces, read from the .bsp (worldscreen_bsp.cpp) in the brush model's own coordinates. Each face's panel
// coordinates come from its texinfo (one texture repeat = the whole panel, top-left of the texture = top-left of the
// panel), so the mapper's alignment stretches, skews and splits the panel over several faces. Every frame the faces
// are moved with the carrier's interpolated origin and angles (cl_entity_t; the engine's brush model rotation:
// AngleVectors axes forward, left, up, without the studio models' pitch inversion). A carrier that is not in the
// current packet (out of the PVS) or has EF_NODRAW shows nothing and can't be aimed at. Aim: the view ray in the
// carrier's frame against each face (front side, within the input distance of its plane, inside the polygon);
// the texinfo at the hit point gives the panel pixel. The faces themselves are invisible in the game (the server
// sets the carrier's render mode to Solid, which alpha-tests the tool texture's palette index 255 away); the
// engine draws the carrier in its transparent pass, after this one, so its other faces hide the panel by depth.
//
// Aim: v_origin / v_angles from view.cpp (set by V_CalcRefdef this frame, before HUD_DrawNormalTriangles); v_angles
// is the aim (stored before the third-person camera offset). Not while dead, spectating, in intermission, or while
// a VGUI2 window has the mouse cursor. While a click is held, the screen keeps the aim (drag), as in Source.
//
// Visibility (worldscreen.h, 5.), per screen and view drawn: the quad is drawn, and counts as seen, only from its
// front side, with a corner inside the engine's view frustum (triangle API WorldToScreen) and the screen's box in
// the view's PVS (triangle API BoxInPVS). It is repainted only when seen within worldscreen_update_dist and not
// hidden by the world: point traces (world only) from the eye to the four corners and the center, at most
// worldscreen_rate times a second; one clear trace is enough.
//
// SBO world: compiled with SBO's client include paths + sbo_compat.h. No Source SDK headers.
//===============================================================//

#include <vector> // before cl_util.h (min / max macros)

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "event_api.h"
#include "parsemsg.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "triangleapi.h"
#include "com_model.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vgui2/vgui2_api.h"
#include "vgui_screen_shared.h"
#include "video_shared.h" // game_shared: movie names (VIDEO_MAX_PATH)
#include "worldscreen.h"
#include "worldscreen_bsp.h"
#include "worldscreen_gl.h"
#include "worldscreen_video.h"

extern vec3_t v_origin, v_angles; // view.cpp: last view origin / aim angles
void HUD_SetCrosshairHidden(bool hidden); // hud_scale.cpp
bool ServerMenu_HidesHud(); // vgui2/sbo_server_menu_bridge.cpp

namespace
{
cvar_t *s_pDebug;
cvar_t *s_pRate;
cvar_t *s_pUpdateDist;

// A bound screen's face: model coordinates from the .bsp, world coordinates for this frame
struct BoundFace
{
	std::vector<float> local;  // corners, 3 floats each
	std::vector<float> uv;     // panel coordinates of the corners (shifted by whole repeats)
	std::vector<float> world;  // corners moved with the carrier, this frame
	float normal[3] = {};      // front side, model coordinates
	float dist = 0;
	float vecs[2][4] = {};     // texinfo
	float invW = 0, invH = 0;  // 1 / texture size
	float shiftU = 0, shiftV = 0; // whole repeats subtracted from u, v
	bool repeat = false;       // the coordinates leave 0..1: drawn with GL_REPEAT, aimed at with the fraction
	float center[3] = {};      // corner average, model coordinates
	float worldNormal[3] = {}, worldCenter[3] = {};
};

struct Screen
{
	bool used = false;
	int entindex = 0;
	int flags = 0;                 // WSCREEN_FLAG_*
	char panel[WSCREEN_MAX_PANELNAME] = "";
	bool hasPanel = false;         // the VGUI2 panel exists (layout found), or the movie opened
	bool movie = false;            // WSCREEN_FLAG_MOVIE: shows a movie (worldscreen_video.cpp), no panel
	int pixelWide = 0, pixelTall = 0;
	float inputDist = 0;
	float corners[4][3] = {};      // top-left, top-right, bottom-right, bottom-left
	float center[3] = {};
	float normal[3] = {};          // unit, front side
	float mins[3] = {}, maxs[3] = {};
	// bound mode (WSCREEN_FLAG_BOUND): center, mins, maxs are this frame's (UpdateCarrier)
	bool bound = false;
	int carrier = 0;               // entity index of the carrier brush entity
	int submodel = 0;              // its model "*N"
	std::vector<BoundFace> faces;
	bool present = false;          // the carrier is in this frame (current packet, drawn): the faces are moved
	float carOrigin[3] = {};       // the carrier's transform this frame: world = origin + axes * local
	float carAxes[3][3] = {};      // forward, left, up
	// visibility
	bool seen = false;             // seen by a view since the host's last ConsumeSeen
	bool occluded = false;         // last trace result
	double lastOcclusionCheck = -1.0;
};
Screen s_Screens[WORLDSCREEN_MAX];

bool s_bVGui2 = false;
bool s_bWarnedNoFbo = false;
bool s_bWarnedFull = false;
int s_iRepaintsLeft = WORLDSCREEN_MAX_REPAINTS_PER_FRAME;

// Fair repaint budget (ClaimRepaint): the hosts ask in slot order, so the frame's repaints are reserved for the
// screens that waited longest among last frame's unserved requests (no content yet first, then the oldest last
// repaint): no slot can starve behind lower ones.
struct RepaintRequest
{
	bool pending = false;   // asked and not served since
	bool noContent = false;
	double lastRender = -1; // the host's clock; -1 never
};
RepaintRequest s_Requests[WORLDSCREEN_MAX];
bool s_bReserved[WORLDSCREEN_MAX];
int s_iReservedLeft = 0;

// Before / longer waiting than b
bool WaitedLonger(const RepaintRequest &a, const RepaintRequest &b)
{
	if (a.noContent != b.noContent)
		return a.noContent;
	return a.lastRender < b.lastRender;
}

// Start of a frame: this frame's repaints are reserved for the longest-waiting requests of the last one.
void ReserveRepaints()
{
	s_iReservedLeft = 0;
	for (int i = 0; i < WORLDSCREEN_MAX; i++)
		s_bReserved[i] = false;
	for (int n = 0; n < WORLDSCREEN_MAX_REPAINTS_PER_FRAME; n++)
	{
		int best = -1;
		for (int i = 0; i < WORLDSCREEN_MAX; i++)
		{
			if (s_Requests[i].pending && !s_bReserved[i] && (best < 0 || WaitedLonger(s_Requests[i], s_Requests[best])))
				best = i;
		}
		if (best < 0)
			break;
		s_bReserved[best] = true;
		s_iReservedLeft++;
	}
	for (int i = 0; i < WORLDSCREEN_MAX; i++)
		s_Requests[i].pending = false; // asked again this frame if still due
}
bool s_bDrawnThisFrame = false;

// aim / click state
int s_iAimSlot = -1;              // the screen the view ray hits (front side, within its input distance), or -1
int s_iAimX = -1, s_iAimY = -1;   // panel pixel (-1: a held click keeps a screen the ray left)
int s_iPressSlot[2] = { -1, -1 }; // [0] attack, [1] attack2: the screen that got the press
double s_flLastAimPrint = -1.0;

float CvarValue(cvar_t *cv, float def)
{
	return cv ? cv->value : def;
}

void Sub(const float *a, const float *b, float *out)
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

float Dot(const float *a, const float *b)
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void Cross(const float *a, const float *b, float *out)
{
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

bool Usable(const Screen &s)
{
	return s.used && s.hasPanel && (s.flags & WSCREEN_FLAG_ACTIVE) && (!s.bound || s.present);
}

//-----------------------------------------------------------------------------
// Bound screens
//-----------------------------------------------------------------------------

// The {screen faces of the screen's carrier model, with their panel coordinates.
void BuildBound(Screen &s, int entindex)
{
	s.faces.clear();
	s.present = false;
	std::vector<wsbsp::Face> faces;
	if (s.submodel <= 0 || s.carrier <= 0)
		return; // the server's carrier is gone
	if (!wsbsp::GetFaces(s.submodel, faces))
		return; // the console has the reason
	if (faces.empty())
	{
		worldscreen::Printf("worldscreen: vgui_screen %d: its carrier (model *%d) has no %s face, not shown\n", entindex,
		    s.submodel, WSCREEN_TOOL_TEXTURE);
		return;
	}

	for (const wsbsp::Face &f : faces)
	{
		BoundFace b;
		const int n = (int)f.xyz.size() / 3;
		b.local = f.xyz;
		b.world.resize(f.xyz.size());
		b.uv.resize(n * 2);
		for (int k = 0; k < 3; k++)
			b.normal[k] = f.normal[k];
		b.dist = f.dist;
		memcpy(b.vecs, f.vecs, sizeof(b.vecs));
		b.invW = 1.0f / f.texWide;
		b.invH = 1.0f / f.texTall;
		float minU = 1e30f, maxU = -1e30f, minV = 1e30f, maxV = -1e30f;
		for (int i = 0; i < n; i++)
		{
			const float *p = &f.xyz[i * 3];
			const float u = (Dot(p, f.vecs[0]) + f.vecs[0][3]) * b.invW;
			const float v = (Dot(p, f.vecs[1]) + f.vecs[1][3]) * b.invH;
			b.uv[i * 2] = u;
			b.uv[i * 2 + 1] = v;
			minU = fminf(minU, u);
			maxU = fmaxf(maxU, u);
			minV = fminf(minV, v);
			maxV = fmaxf(maxV, v);
			for (int k = 0; k < 3; k++)
				b.center[k] += p[k] / n;
		}
		// whole repeats off (a face fitted to the texture then has 0..1 and is drawn clamped: no wrapped seam)
		b.shiftU = floorf(minU + 0.001f);
		b.shiftV = floorf(minV + 0.001f);
		b.repeat = maxU - b.shiftU > 1.001f || maxV - b.shiftV > 1.001f;
		for (int i = 0; i < n; i++)
		{
			b.uv[i * 2] -= b.shiftU;
			b.uv[i * 2 + 1] -= b.shiftV;
		}
		s.faces.push_back(b);
	}
}

void ToWorld(const Screen &s, const float *local, float *world)
{
	for (int i = 0; i < 3; i++)
		world[i] = s.carOrigin[i] + s.carAxes[0][i] * local[0] + s.carAxes[1][i] * local[1] + s.carAxes[2][i] * local[2];
}

void DirToLocal(const Screen &s, const float *dir, float *local)
{
	for (int i = 0; i < 3; i++)
		local[i] = Dot(dir, s.carAxes[i]);
}

void PointToLocal(const Screen &s, const float *world, float *local)
{
	float d[3];
	Sub(world, s.carOrigin, d);
	DirToLocal(s, d, local);
}

// The carrier's transform: the engine's brush model rotation (R_RotateForEntity; no pitch inversion as for studio
// models): AngleVectors forward, -right (left), up.
void CarrierTransform(const cl_entity_t *e, float *origin, float axes[3][3])
{
	vec3_t ang = { e->angles[0], e->angles[1], e->angles[2] };
	vec3_t fwd, right, up;
	gEngfuncs.pfnAngleVectors(ang, fwd, right, up);
	for (int i = 0; i < 3; i++)
	{
		origin[i] = e->origin[i];
		axes[0][i] = fwd[i];
		axes[1][i] = -right[i];
		axes[2][i] = up[i];
	}
}

// The carrier entity of this frame, or nullptr: not in the current packet (out of the PVS), not drawn, or not the
// brush model the server named (the entity slot was reused).
cl_entity_t *CarrierEntity(const Screen &s)
{
	if (!s.bound || s.carrier <= 0 || s.faces.empty())
		return nullptr;
	cl_entity_t *e = gEngfuncs.GetEntityByIndex(s.carrier);
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	if (!e || !local || !e->model || e->model->name[0] != '*' || atoi(e->model->name + 1) != s.submodel)
		return nullptr;
	if (e->curstate.messagenum != local->curstate.messagenum || (e->curstate.effects & EF_NODRAW))
		return nullptr;
	return e;
}

// This frame's place of a bound screen's faces (and its center and box for the visibility tests).
void UpdateCarrier(Screen &s)
{
	const cl_entity_t *e = CarrierEntity(s);
	s.present = e != nullptr;
	if (!e)
		return;
	CarrierTransform(e, s.carOrigin, s.carAxes);

	bool first = true;
	for (BoundFace &f : s.faces)
	{
		const int n = (int)f.local.size() / 3;
		for (int i = 0; i < n; i++)
		{
			float *w = &f.world[i * 3];
			ToWorld(s, &f.local[i * 3], w);
			for (int k = 0; k < 3; k++)
			{
				if (first)
					s.mins[k] = s.maxs[k] = w[k];
				s.mins[k] = fminf(s.mins[k], w[k]);
				s.maxs[k] = fmaxf(s.maxs[k], w[k]);
			}
			first = false;
		}
		ToWorld(s, f.center, f.worldCenter);
		for (int k = 0; k < 3; k++)
			f.worldNormal[k] = s.carAxes[0][k] * f.normal[0] + s.carAxes[1][k] * f.normal[1] + s.carAxes[2][k] * f.normal[2];
	}
	for (int k = 0; k < 3; k++)
	{
		s.center[k] = (s.mins[k] + s.maxs[k]) * 0.5f;
		s.mins[k] -= 1.0f; // 1 unit thick at least, for the PVS test
		s.maxs[k] += 1.0f;
	}
}

// p (model coordinates, in the face's plane or near it) inside the face's polygon, with `slack` units outside its
// edges allowed. Convex polygon, either winding.
bool InsidePolygon(const BoundFace &f, const float *p, float slack)
{
	const int n = (int)f.local.size() / 3;
	int pos = 0, neg = 0;
	for (int i = 0; i < n; i++)
	{
		const float *a = &f.local[i * 3];
		const float *b = &f.local[((i + 1) % n) * 3];
		float e[3], w[3], c[3];
		Sub(b, a, e);
		Sub(p, a, w);
		Cross(e, w, c);
		const float len = sqrtf(Dot(e, e));
		if (len < 1e-4f)
			continue;
		const float d = Dot(c, f.normal) / len; // distance from the edge's line, signed by the winding
		if (d > slack)
			pos++;
		else if (d < -slack)
			neg++;
	}
	return pos == 0 || neg == 0;
}

// The panel pixel of model point p on face f.
void PanelPixel(const Screen &s, const BoundFace &f, const float *p, int &px, int &py)
{
	float u = (Dot(p, f.vecs[0]) + f.vecs[0][3]) * f.invW - f.shiftU;
	float v = (Dot(p, f.vecs[1]) + f.vecs[1][3]) * f.invH - f.shiftV;
	if (f.repeat)
	{
		u -= floorf(u);
		v -= floorf(v);
	}
	px = (int)(u * s.pixelWide);
	py = (int)(v * s.pixelTall);
	px = px < 0 ? 0 : (px >= s.pixelWide ? s.pixelWide - 1 : px);
	py = py < 0 ? 0 : (py >= s.pixelTall ? s.pixelTall - 1 : py);
}

// TraceScreen for a bound screen: the view ray in the carrier's frame against each face.
bool TraceBound(const Screen &s, const float *eye, const float *dir, int &px, int &py, float &dist)
{
	float eyeL[3], dirL[3];
	PointToLocal(s, eye, eyeL);
	DirToLocal(s, dir, dirL);
	bool hit = false;
	for (const BoundFace &f : s.faces)
	{
		const float planeDist = Dot(eyeL, f.normal) - f.dist;
		if (planeDist < 0 || planeDist > s.inputDist)
			continue;
		const float denom = Dot(dirL, f.normal);
		if (denom >= -1e-6f)
			continue;
		const float t = -planeDist / denom;
		if (hit && t >= dist)
			continue;
		float p[3];
		for (int i = 0; i < 3; i++)
			p[i] = eyeL[i] + dirL[i] * t;
		if (!InsidePolygon(f, p, 0.0f))
			continue;
		PanelPixel(s, f, p, px, py);
		dist = t;
		hit = true;
	}
	return hit;
}

// The view ray against a screen. True with the panel pixel and the ray length if it hits the front side and the
// eye is within the screen's input distance of its plane.
bool TraceScreen(const Screen &s, const float *eye, const float *dir, int &px, int &py, float &dist)
{
	if (s.bound)
		return TraceBound(s, eye, dir, px, py, dist);
	float toEye[3];
	Sub(eye, s.corners[0], toEye);
	const float planeDist = Dot(toEye, s.normal);
	if (planeDist < 0 || planeDist > s.inputDist)
		return false;
	const float denom = Dot(dir, s.normal);
	if (denom >= -1e-6f) // parallel, or looking away from the front side
		return false;
	const float t = -planeDist / denom;

	float u[3], v[3], rel[3];
	Sub(s.corners[1], s.corners[0], u); // top-left -> top-right
	Sub(s.corners[3], s.corners[0], v); // top-left -> bottom-left
	for (int i = 0; i < 3; i++)
		rel[i] = eye[i] + dir[i] * t - s.corners[0][i];
	const float fs = Dot(rel, u) / Dot(u, u);
	const float ft = Dot(rel, v) / Dot(v, v);
	if (fs < 0 || fs >= 1 || ft < 0 || ft >= 1)
		return false;

	px = (int)(fs * s.pixelWide);
	py = (int)(ft * s.pixelTall);
	dist = t;
	return true;
}

// Front side, and a corner inside the view frustum. WorldToScreen is the engine's own projection of this view
// (field of view and aspect as drawn); it returns 1 for a point behind the eye, whose x/y are then meaningless.
// Bound screens: in front of at least one face, the corners of the faces' box.
bool InView(const Screen &s, const float *eye)
{
	float toEye[3];
	float points[8][3];
	int numPoints = 4;
	if (s.bound)
	{
		bool front = false;
		for (size_t i = 0; i < s.faces.size() && !front; i++)
		{
			Sub(eye, s.faces[i].worldCenter, toEye);
			front = Dot(toEye, s.faces[i].worldNormal) > 0;
		}
		if (!front)
			return false;
		numPoints = 8;
		for (int i = 0; i < 8; i++)
		{
			points[i][0] = (i & 1) ? s.maxs[0] : s.mins[0];
			points[i][1] = (i & 2) ? s.maxs[1] : s.mins[1];
			points[i][2] = (i & 4) ? s.maxs[2] : s.mins[2];
		}
	}
	else
	{
		Sub(eye, s.center, toEye);
		if (Dot(toEye, s.normal) <= 0)
			return false; // behind the screen (or in its plane)
		memcpy(points, s.corners, sizeof(s.corners));
	}

	int behind = 0, left = 0, right = 0, below = 0, above = 0;
	for (int i = 0; i < numPoints; i++)
	{
		float corner[3] = { points[i][0], points[i][1], points[i][2] };
		float scr[3];
		if (gEngfuncs.pTriAPI->WorldToScreen(corner, scr))
		{
			behind++;
			continue;
		}
		left += scr[0] < -1.0f;
		right += scr[0] > 1.0f;
		below += scr[1] < -1.0f;
		above += scr[1] > 1.0f;
	}
	if (behind == numPoints)
		return false;
	// all corners in front, all off the same side: outside (a corner behind the eye: can't tell, so visible)
	if (behind == 0 && (left == numPoints || right == numPoints || below == numPoints || above == numPoints))
		return false;

	// Steam engine addition to the triangle API (like FogParams in tri.cpp)
	float mins[3] = { s.mins[0], s.mins[1], s.mins[2] };
	float maxs[3] = { s.maxs[0], s.maxs[1], s.maxs[2] };
	if (gEngfuncs.pTriAPI->BoxInPVS && !gEngfuncs.pTriAPI->BoxInPVS(mins, maxs))
		return false;
	return true;
}

// Point traces against the world from the eye to the corners and the center, each 2 units in front of the screen (so
// a wall it hangs on doesn't count). Hidden only when all of them hit something; an eye inside a solid (noclip)
// sees it.
// Bound screens: the centers of the first five faces, 2 units in front of each.
bool Occluded(const Screen &s, const float *eye)
{
	float points[5][3];
	int numPoints = 5;
	if (s.bound)
	{
		numPoints = s.faces.size() < 5 ? (int)s.faces.size() : 5;
		for (int k = 0; k < numPoints; k++)
		{
			for (int i = 0; i < 3; i++)
				points[k][i] = s.faces[k].worldCenter[i] + s.faces[k].worldNormal[i] * 2.0f;
		}
	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			for (int k = 0; k < 4; k++)
				points[k][i] = s.corners[k][i] + s.normal[i] * 2.0f;
			points[4][i] = s.center[i] + s.normal[i] * 2.0f;
		}
	}

	float start[3] = { eye[0], eye[1], eye[2] };
	gEngfuncs.pEventAPI->EV_SetTraceHull(2); // point
	for (int k = 0; k < numPoints; k++)
	{
		pmtrace_t tr;
		gEngfuncs.pEventAPI->EV_PlayerTrace(start, points[k], PM_WORLD_ONLY, -1, &tr);
		if (tr.startsolid || tr.fraction >= 1.0f)
			return false;
	}
	return true;
}

// The texture is worth repainting for this view: close enough and not hidden by the world (traces at the update rate).
bool WorthRepaint(int slot, const float *eye)
{
	Screen &s = s_Screens[slot];
	float toEye[3];
	Sub(eye, s.center, toEye);
	const float maxDist = CvarValue(s_pUpdateDist, 1024);
	if (maxDist > 0 && Dot(toEye, toEye) > maxDist * maxDist)
		return false;

	if (slot == s_iAimSlot)
		return true; // the cursor is on it

	const double now = gEngfuncs.GetClientTime();
	if (s.lastOcclusionCheck < 0 || now < s.lastOcclusionCheck ||
	    now - s.lastOcclusionCheck >= 1.0 / worldscreen::UpdateRate())
	{
		s.lastOcclusionCheck = now;
		const bool was = s.occluded;
		s.occluded = Occluded(s, eye);
		if (was != s.occluded && worldscreen::DebugLevel() > 0)
			worldscreen::Printf("worldscreen: screen %d (entity %d) %s by the world\n", slot, s.entindex,
			    s.occluded ? "hidden" : "no longer hidden");
	}
	return !s.occluded;
}

void SetAim(int slot, int px, int py)
{
	if (slot == s_iAimSlot && px == s_iAimX && py == s_iAimY)
		return;
	if (s_iAimSlot >= 0 && slot != s_iAimSlot)
	{
		if (worldscreen::DebugLevel() > 0)
			worldscreen::Printf("worldscreen: aim left screen %d\n", s_iAimSlot);
		if (s_bVGui2)
			VGui2_WorldScreen_SetCursor(s_iAimSlot, false, 0, 0);
	}
	const bool entered = slot >= 0 && slot != s_iAimSlot;
	s_iAimSlot = slot;
	s_iAimX = px;
	s_iAimY = py;
	if (slot < 0)
		return;
	if (s_bVGui2)
		VGui2_WorldScreen_SetCursor(slot, px >= 0, px, py);

	if (worldscreen::DebugLevel() > 0)
	{
		const double now = gEngfuncs.GetClientTime();
		if (entered || s_flLastAimPrint < 0 || now < s_flLastAimPrint || now - s_flLastAimPrint >= 0.5)
		{
			s_flLastAimPrint = now;
			worldscreen::Printf("worldscreen: aim%s at screen %d (entity %d) panel (%d, %d)\n",
			    entered ? " entered," : "", slot, s_Screens[slot].entindex, px, py);
		}
	}
}

// The view of this frame: which screen does the player aim at?
void UpdateAim()
{
	// dead, spectators, intermission, a VGUI2 window with the mouse cursor: no aim (a held click keeps its screen)
	const bool canAim = !gHUD.m_fPlayerDead && !g_iUser1 && !gHUD.m_iIntermission && !VGui2_IsCursorVisible();
	const int held = s_iPressSlot[0] >= 0 ? s_iPressSlot[0] : s_iPressSlot[1];

	vec3_t fwd, right, up;
	gEngfuncs.pfnAngleVectors(v_angles, fwd, right, up);
	const float eye[3] = { v_origin[0], v_origin[1], v_origin[2] };
	const float dir[3] = { fwd[0], fwd[1], fwd[2] };

	int best = -1, bx = -1, by = -1;
	float bestDist = 0;
	for (int i = 0; i < WORLDSCREEN_MAX; i++)
	{
		// display-only screens (spawnflag "Input Disabled") are never aimed at: no cursor, fire works as usual
		if (!canAim || !Usable(s_Screens[i]) || (s_Screens[i].flags & WSCREEN_FLAG_NOINPUT) || (held >= 0 && i != held))
			continue;
		int px, py;
		float dist;
		if (TraceScreen(s_Screens[i], eye, dir, px, py, dist) && (best < 0 || dist < bestDist))
		{
			best = i;
			bx = px;
			by = py;
			bestDist = dist;
		}
	}
	if (best < 0 && held >= 0 && Usable(s_Screens[held]))
		best = held; // drag: the screen keeps the aim until the click is released, the cursor is off it
	SetAim(best, bx, by);
}

void BuildCorners(Screen &s, const float *origin, const float *angles, float width, float height)
{
	vec3_t ang = { angles[0], angles[1], angles[2] };
	vec3_t fwd, right, up;
	gEngfuncs.pfnAngleVectors(ang, fwd, right, up);
	for (int i = 0; i < 3; i++)
	{
		const float w = -right[i] * width; // the viewer's right
		const float h = up[i] * height;
		s.corners[3][i] = origin[i];             // bottom-left
		s.corners[2][i] = origin[i] + w;         // bottom-right
		s.corners[0][i] = origin[i] + h;         // top-left
		s.corners[1][i] = origin[i] + h + w;     // top-right
		s.center[i] = origin[i] + (w + h) * 0.5f;
		s.normal[i] = fwd[i];
	}
	for (int i = 0; i < 3; i++)
	{
		s.mins[i] = s.maxs[i] = s.corners[0][i];
		for (int k = 1; k < 4; k++)
		{
			s.mins[i] = fminf(s.mins[i], s.corners[k][i]);
			s.maxs[i] = fmaxf(s.maxs[i], s.corners[k][i]);
		}
		s.mins[i] -= 1.0f; // 1 unit thick, for the PVS test
		s.maxs[i] += 1.0f;
	}
}

int FindSlot(int entindex)
{
	for (int i = 0; i < WORLDSCREEN_MAX; i++)
	{
		if (s_Screens[i].used && s_Screens[i].entindex == entindex)
			return i;
	}
	return -1;
}

void ReleaseSlot(int slot)
{
	if (slot == s_iAimSlot)
		SetAim(-1, -1, -1);
	for (int b = 0; b < 2; b++)
	{
		if (s_iPressSlot[b] == slot)
			s_iPressSlot[b] = -1; // its release goes nowhere: IN_Attack*Up finds no press
	}
}

void RemoveScreen(int slot)
{
	ReleaseSlot(slot);
	if (s_bVGui2 && s_Screens[slot].used && !s_Screens[slot].movie)
		VGui2_WorldScreen_Remove(slot);
	wsvideo::Remove(slot);
	s_Screens[slot] = Screen();
}

void Define()
{
	const int entindex = READ_SHORT();
	float origin[3], angles[3];
	for (int i = 0; i < 3; i++)
		origin[i] = READ_COORD();
	for (int i = 0; i < 3; i++)
		angles[i] = READ_SHORT() * (360.0f / 65536.0f);
	const float width = READ_COORD();
	const float height = READ_COORD();
	const int inputDist = READ_SHORT();
	const int flags = READ_BYTE();
	char panel[WSCREEN_MAX_PANELNAME];
	strncpy(panel, READ_STRING(), sizeof(panel) - 1);
	panel[sizeof(panel) - 1] = 0;
	int carrier = 0, submodel = 0;
	if (flags & WSCREEN_FLAG_BOUND)
	{
		carrier = READ_SHORT();
		submodel = READ_SHORT();
	}
	char movie[VIDEO_MAX_PATH] = "";
	float elapsed = 0;
	int volume = 10, radius = 1250;
	if (flags & WSCREEN_FLAG_MOVIE)
	{
		strncpy(movie, READ_STRING(), sizeof(movie) - 1);
		movie[sizeof(movie) - 1] = 0;
		elapsed = READ_LONG() / 1000.0f;
		volume = READ_BYTE();
		radius = READ_SHORT();
	}
	if (!WScreen_ValidName(panel, WSCREEN_MAX_PANELNAME, 0))
		return;

	int slot = FindSlot(entindex);
	if (slot < 0)
	{
		for (int i = 0; i < WORLDSCREEN_MAX && slot < 0; i++)
		{
			if (!s_Screens[i].used)
				slot = i;
		}
		if (slot < 0)
		{
			if (!s_bWarnedFull)
				worldscreen::Printf("worldscreen: the map has more than %d vgui_screen entities, entity %d and later "
				                    "ones are not shown\n", WORLDSCREEN_MAX, entindex);
			s_bWarnedFull = true;
			return;
		}
	}

	Screen &s = s_Screens[slot];
	const bool newPanel = !s.used || strcmp(s.panel, panel) != 0;
	s.used = true;
	s.entindex = entindex;
	s.flags = flags;
	s.inputDist = (float)inputDist;
	BuildCorners(s, origin, angles, width, height);
	const bool bound = (flags & WSCREEN_FLAG_BOUND) != 0;
	if (bound != s.bound || carrier != s.carrier || submodel != s.submodel)
	{
		s.bound = bound;
		s.carrier = carrier;
		s.submodel = submodel;
		s.faces.clear();
		s.present = false;
		if (bound)
			BuildBound(s, entindex);
	}
	if (flags & WSCREEN_FLAG_MOVIE)
	{
		// a movie instead of a panel (worldscreen_video.cpp checks the name and opens the file)
		if (s.hasPanel && !s.movie && s_bVGui2)
			VGui2_WorldScreen_Remove(slot);
		if (!s.movie)
			ReleaseSlot(slot);
		s.movie = true;
		strcpy(s.panel, panel);
		s.hasPanel = wsvideo::Define(slot, movie, elapsed, flags, volume, radius, s.pixelWide, s.pixelTall);
	}
	else if (newPanel || s.movie)
	{
		if (s.movie)
			wsvideo::Remove(slot);
		s.movie = false;
		ReleaseSlot(slot);
		strcpy(s.panel, panel);
		s.hasPanel = s_bVGui2 && VGui2_WorldScreen_Create(slot, panel, s.pixelWide, s.pixelTall);
		if (!s.hasPanel)
			worldscreen::Printf("worldscreen: vgui_screen %d: no layout ui/scripts/screens/%s.res, not shown\n",
			    entindex, panel);
	}
	if (!(flags & WSCREEN_FLAG_ACTIVE))
		ReleaseSlot(slot);
	if (worldscreen::DebugLevel() > 0 && s.bound)
		worldscreen::Printf("worldscreen: screen %d = entity %d \"%s\" %dx%d px on carrier entity %d (model *%d, %d "
		                    "%s faces), input distance %d, flags %d\n",
		    slot, entindex, panel, s.pixelWide, s.pixelTall, carrier, submodel, (int)s.faces.size(), WSCREEN_TOOL_TEXTURE,
		    inputDist, flags);
	else if (worldscreen::DebugLevel() > 0)
		worldscreen::Printf("worldscreen: screen %d = entity %d \"%s\" %dx%d px, %.0fx%.0f units at %.0f %.0f %.0f, "
		                    "angles %.0f %.0f %.0f, input distance %d, flags %d\n",
		    slot, entindex, panel, s.pixelWide, s.pixelTall, width, height, origin[0], origin[1], origin[2], angles[0],
		    angles[1], angles[2], inputDist, flags);
}

int MsgFunc_WScreen(const char *, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const int op = READ_BYTE();
	if (op == WSCREEN_OP_DEFINE)
	{
		Define();
		return 1;
	}

	const int slot = FindSlot(READ_SHORT());
	if (op == WSCREEN_OP_STATE)
	{
		const int flags = READ_BYTE();
		if (slot < 0)
			return 1;
		s_Screens[slot].flags = flags;
		if (s_Screens[slot].movie)
			wsvideo::SetFlags(slot, flags); // active again: the movie starts over
		if (!(flags & WSCREEN_FLAG_ACTIVE))
			ReleaseSlot(slot);
		if (worldscreen::DebugLevel() > 0)
			worldscreen::Printf("worldscreen: screen %d %s\n", slot, (flags & WSCREEN_FLAG_ACTIVE) ? "active" : "inactive");
	}
	else if (op == WSCREEN_OP_CONTROL)
	{
		const int controlOp = READ_BYTE();
		char control[WSCREEN_MAX_CONTROLNAME];
		strncpy(control, READ_STRING(), sizeof(control) - 1);
		control[sizeof(control) - 1] = 0;
		const char *value = READ_STRING();
		// an animation: "control" is its state group (may be empty), the value the sequence name
		const bool ok = controlOp == WSCREEN_CONTROL_ANIMATION
		                    ? (!control[0] || WScreen_ValidName(control, WSCREEN_MAX_CONTROLNAME, 0)) &&
		                          WScreen_ValidName(value, WSCREEN_MAX_CONTROLNAME, 0)
		                    : WScreen_ValidName(control, WSCREEN_MAX_CONTROLNAME, 0);
		if (slot >= 0 && s_Screens[slot].hasPanel && ok)
			VGui2_WorldScreen_Control(slot, controlOp, control, value);
	}
	return 1;
}

void Status_f()
{
	for (int i = 0; i < WORLDSCREEN_MAX; i++)
	{
		const Screen &s = s_Screens[i];
		if (!s.used)
			continue;
		worldscreen::Printf("screen %d: entity %d \"%s\" %s, panel %s %dx%d, renders %d, hidden by the world %d\n", i,
		    s.entindex, s.panel, (s.flags & WSCREEN_FLAG_ACTIVE) ? "active" : "inactive", s.hasPanel ? "yes" : "NO",
		    s.pixelWide, s.pixelTall, s.hasPanel ? VGui2_WorldScreen_RenderCount(i) : 0, s.occluded);
		if (s.bound)
			worldscreen::Printf("  bound: carrier entity %d, model *%d, %d %s faces, carrier in view %d, box %.0f %.0f "
			                    "%.0f .. %.0f %.0f %.0f\n", s.carrier, s.submodel, (int)s.faces.size(),
			    WSCREEN_TOOL_TEXTURE, s.present, s.mins[0], s.mins[1], s.mins[2], s.maxs[0], s.maxs[1], s.maxs[2]);
	}
	wsvideo::PrintStatus();
	worldscreen::Printf("worldscreen: VGUI2 %d, aimed at screen %d (%d, %d), click held on %d / %d\n", s_bVGui2,
	    s_iAimSlot, s_iAimX, s_iAimY, s_iPressSlot[0], s_iPressSlot[1]);
	wsgl::PrintStatus();
}
} // namespace

namespace worldscreen
{
int DebugLevel()
{
	return (int)CvarValue(s_pDebug, 0);
}

float UpdateRate()
{
	float r = CvarValue(s_pRate, 10);
	if (r < 0.5f)
		r = 0.5f;
	if (r > 100.0f)
		r = 100.0f;
	return r;
}

bool ConsumeSeen(int slot)
{
	if (slot < 0 || slot >= WORLDSCREEN_MAX)
		return false;
	Screen &s = s_Screens[slot];
	const bool seen = s.seen;
	s.seen = false;
	return seen && Usable(s);
}

bool ClaimRepaint(int slot, bool hasContent, double lastRender)
{
	if (slot < 0 || slot >= WORLDSCREEN_MAX)
		return false;
	RepaintRequest &r = s_Requests[slot];
	r.pending = true;
	r.noContent = !hasContent;
	r.lastRender = lastRender;
	if (s_iRepaintsLeft <= 0)
		return false;
	if (s_bReserved[slot])
	{
		s_bReserved[slot] = false;
		s_iReservedLeft--;
	}
	else if (s_iRepaintsLeft <= s_iReservedLeft)
		return false; // kept for screens that waited longer
	s_iRepaintsLeft--;
	r.pending = false;
	return true;
}

void SendCommand(int slot, const char *command)
{
	if (slot < 0 || slot >= WORLDSCREEN_MAX || !Usable(s_Screens[slot]) || !command)
		return;
	if (!WScreen_ValidName(command, WSCREEN_MAX_COMMAND, 1))
	{
		Printf("worldscreen: button command \"%s\" not sent: at most %d letters, digits, '_', '-', '.', ':' and "
		       "single spaces\n", command, WSCREEN_MAX_COMMAND - 1);
		return;
	}
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "vgui_screen_command %d \"%s\"", s_Screens[slot].entindex, command);
	if (DebugLevel() > 0)
		Printf("worldscreen: %s\n", cmd);
	gEngfuncs.pfnServerCmd(cmd);
}

void Printf(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	buf[sizeof(buf) - 1] = 0;
	gEngfuncs.pfnConsolePrint(buf);
}
} // namespace worldscreen

void WorldScreen_Init()
{
	s_pDebug = gEngfuncs.pfnRegisterVariable((char *)"worldscreen_debug", (char *)"0", 0);
	s_pRate = gEngfuncs.pfnRegisterVariable((char *)"worldscreen_rate", (char *)"10", 0);
	s_pUpdateDist = gEngfuncs.pfnRegisterVariable((char *)"worldscreen_update_dist", (char *)"1024", 0);
	gEngfuncs.pfnAddCommand((char *)"worldscreen_status", Status_f);
	gEngfuncs.pfnHookUserMsg((char *)"WScreen", MsgFunc_WScreen);

	s_bVGui2 = VGui2_IsAvailable() && VGui2_WorldScreen_Init();
}

void WorldScreen_Reset()
{
	for (int i = 0; i < WORLDSCREEN_MAX; i++)
		RemoveScreen(i);
	wsvideo::RemoveAll();
	s_bWarnedFull = false;
	wsbsp::Reset();
}

bool WorldScreen_Attack(int button, bool down)
{
	if (button < 0 || button > 1)
		return false;

	if (down)
	{
		if (s_iPressSlot[button] >= 0)
			return true; // a second key bound to the same attack
		if (s_iAimSlot < 0)
			return false; // not aimed at a screen: attack as usual
		s_iPressSlot[button] = s_iAimSlot;
		if (worldscreen::DebugLevel() > 0)
			worldscreen::Printf("worldscreen: %s press on screen %d at (%d, %d)\n", button ? "right" : "left",
			    s_iAimSlot, s_iAimX, s_iAimY);
		if (s_bVGui2)
			VGui2_WorldScreen_MouseButton(s_iAimSlot, button, true);
		return true;
	}

	// release: only the one whose press went to a screen (even if no longer aimed at it); others pass through
	const int slot = s_iPressSlot[button];
	if (slot < 0)
		return false;
	s_iPressSlot[button] = -1;
	if (worldscreen::DebugLevel() > 0)
		worldscreen::Printf("worldscreen: %s release on screen %d\n", button ? "right" : "left", slot);
	if (s_bVGui2)
		VGui2_WorldScreen_MouseButton(slot, button, false);
	return true;
}

void WorldScreen_Frame()
{
	// no 3D view this frame (menus, loading): no aim
	if (!s_bDrawnThisFrame && s_iAimSlot >= 0)
		SetAim(-1, -1, -1);
	s_bDrawnThisFrame = false;
	s_iRepaintsLeft = WORLDSCREEN_MAX_REPAINTS_PER_FRAME;
	ReserveRepaints();

	// movie screens' sound, from where they are now
	{
		float centers[WORLDSCREEN_MAX][3];
		bool active[WORLDSCREEN_MAX];
		for (int i = 0; i < WORLDSCREEN_MAX; i++)
		{
			const Screen &s = s_Screens[i];
			memcpy(centers[i], s.center, sizeof(centers[i]));
			active[i] = s.movie && Usable(s);
		}
		wsvideo::Frame(centers, active, WORLDSCREEN_MAX);
	}

	const bool aimed = s_iAimSlot >= 0;
	HUD_SetCrosshairHidden(aimed || ServerMenu_HidesHud()); // no crosshair while a server menu is up either
	if (s_bVGui2)
		VGui2_WorldScreen_ShowCursor(aimed && !(s_Screens[s_iAimSlot].flags & WSCREEN_FLAG_NOCURSOR));
}

void WorldScreen_DrawWorld()
{
	// bound screens: where their carriers are this frame (before the aim)
	for (int i = 0; i < WORLDSCREEN_MAX; i++)
	{
		if (s_Screens[i].used && s_Screens[i].bound)
			UpdateCarrier(s_Screens[i]);
	}

	// the main view only: the spectator's overview and inset are no aim (and spectators can't use screens)
	if (!gHUD.m_Spectator.m_bOverviewDrawPass && !gHUD.m_Spectator.m_bInsetDrawPass)
	{
		UpdateAim();
		s_bDrawnThisFrame = true;
	}

	bool any = false;
	for (int i = 0; i < WORLDSCREEN_MAX && !any; i++)
		any = Usable(s_Screens[i]) || (s_Screens[i].bound && s_Screens[i].present);
	if (!any)
		return;

	if (!wsgl::Init())
	{
		if (!s_bWarnedNoFbo)
		{
			s_bWarnedNoFbo = true;
			worldscreen::Printf("worldscreen: no OpenGL context (software renderer?) or no FBO support (see "
			                    "worldscreen_status); world screens are not drawn\n");
		}
		return;
	}

	// This view (main view, spectator inset or overview): each screen skipped from behind, outside the frustum or
	// the PVS. The VGUI2 panels render during the engine's VGUI2 paint, after this 3D pass, and only if seen here.
	const float eye[3] = { v_origin[0], v_origin[1], v_origin[2] };
	int movieSlots[WORLDSCREEN_MAX], movieCount = 0;
	float movieDist[WORLDSCREEN_MAX];
	for (int i = 0; i < WORLDSCREEN_MAX; i++)
	{
		Screen &s = s_Screens[i];
		// a bound screen that is off (or has no layout) is drawn black: its faces are invisible in the game
		const bool off = s.used && s.bound && s.present && !Usable(s);
		if ((!Usable(s) && !off) || !InView(s, eye))
			continue;
		if (s.movie)
		{
			// a movie is decoded only while seen (close enough, not hidden by the world); its clock runs on
			if (!off && WorthRepaint(i, eye) && movieCount < WORLDSCREEN_MAX)
			{
				float d[3];
				Sub(eye, s.center, d);
				movieSlots[movieCount] = i;
				movieDist[movieCount++] = Dot(d, d);
			}
		}
		else if (!off && !s.seen && WorthRepaint(i, eye))
			s.seen = true;
		if (!s.bound)
		{
			if (s.movie && !wsgl::HasContent(i))
			{
				// a movie screen without a picture yet (not among the nearest decoded): black
				static const float uv[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
				const wsgl::WorldPolygon quad = { &s.corners[0][0], uv, 4, false };
				wsgl::DrawWorldPolygons(i, &quad, 1, true);
				continue;
			}
			wsgl::DrawWorldQuad(i, s.corners); // nothing without content yet
			continue;
		}
		static std::vector<wsgl::WorldPolygon> polys;
		polys.clear();
		for (const BoundFace &f : s.faces)
			polys.push_back({ f.world.data(), f.uv.data(), (int)f.local.size() / 3, f.repeat });
		wsgl::DrawWorldPolygons(i, polys.data(), (int)polys.size(), off);
	}

	// the nearest WORLDSCREEN_MAX_MOVIE_DECODES movie screens seen get their next picture (shown from the next frame
	// on); farther ones keep their last picture and catch up with their clock when they are among the nearest again
	for (int n = 0; n < movieCount && n < WORLDSCREEN_MAX_MOVIE_DECODES; n++)
	{
		int best = n;
		for (int k = n + 1; k < movieCount; k++)
		{
			if (movieDist[k] < movieDist[best])
				best = k;
		}
		const int slot = movieSlots[best];
		const float dist = movieDist[best];
		movieSlots[best] = movieSlots[n];
		movieDist[best] = movieDist[n];
		movieSlots[n] = slot;
		movieDist[n] = dist;
		wsvideo::UpdatePicture(slot);
	}
}

bool WorldScreen_IsScreenFace(int entindex, const float *point)
{
	if (entindex <= 0 || !point)
		return false;
	for (int i = 0; i < WORLDSCREEN_MAX; i++)
	{
		Screen &s = s_Screens[i];
		if (!s.used || !s.bound || s.carrier != entindex || s.faces.empty())
			continue;
		const cl_entity_t *e = gEngfuncs.GetEntityByIndex(entindex);
		if (!e)
			return false;
		// the entity's place now (a decal may come between two 3D frames)
		CarrierTransform(e, s.carOrigin, s.carAxes);
		float p[3];
		PointToLocal(s, point, p);
		for (const BoundFace &f : s.faces)
		{
			if (fabsf(Dot(p, f.normal) - f.dist) <= 1.0f && InsidePolygon(f, p, 1.0f))
				return true;
		}
	}
	return false;
}
