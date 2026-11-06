//
// hud_scale.cpp
//
// HUD scaling: halflife-updated PR #245 "HUD scaling via triangle API" (Toodles2You, twhl-community/halflife-updated
// 1fa21e5), adapted for SBO.
//
// HUD elements lay out in a reference size (gHUD.GetWidth() x gHUD.GetHeight()). With hud_scale 1 (upstream's name
// and default; SBO's default is 0) on a hardware renderer, that size is the screen divided by a scale factor so that a 640x480 HUD fits
// on the screen, and the sprite/fill/text helpers below (cl_util.h) draw at the scaled position and size through the
// triangle API. SBO only ships the 640 sprite set, which is what gets stretched: no new art.
// With hud_scale 0, the software renderer or screens up to 640x480 the reference size is the screen and every helper
// calls the engine directly, exactly as before.
//
// Differences from the PR (see docs/dev/sdk-update/client.md, "HUD scaling"):
//  * scale = min( width / 640, height / 480 ) instead of height / 480, so 5:4 screens keep a >= 640 wide HUD;
//  * the sprite resolution set is still chosen from the real screen width (SBO has 320/640 only);
//  * texture coordinates are inset by half a texel (no bleeding from neighbouring atlas icons / texture wrap);
//  * quad edges are rounded to whole pixels (no seams between adjacent pieces, e.g. digits, scope tiles);
//  * the crosshair follows the main view only (not the spectator PIP inset), keeps the spectator crosshair and
//    projects auto-aim with the engine's screen convention (y up);
//  * console text keeps its native glyph size; positions are scaled and metrics are converted to HUD units.
//
// The sniper scope (sniper.cpp) uses the same sprite quads in screen pixels, with or without hud_scale
// (SPR_DrawTexels).
//

#include <math.h>	// before cl_util.h (its fabs macro)

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "triangleapi.h"
#include "r_studioint.h"
#include "com_model.h"
#include "pm_shared.h"

extern engine_studio_api_t IEngineStudio;

namespace
{
// reference HUD size the 640 sprite set is laid out for
constexpr float kRefWidth = 640.0f;
constexpr float kRefHeight = 480.0f;

float s_flScale = 1.0f;
float s_flOffsetX = 0.0f;
float s_flOffsetY = 0.0f;

// SPR_Set state (also kept while not scaling: used when hud_scale is switched on at the next VidInit)
HSPRITE s_hPic = 0;
int s_iPicR = 255, s_iPicG = 255, s_iPicB = 255;

// SetCrosshair state
HSPRITE s_hCrosshair = 0;
wrect_t s_rcCrosshair = { 0, 0, 0, 0 };
int s_iCrosshairR = 255, s_iCrosshairG = 255, s_iCrosshairB = 255;
// SBO 2.0 world screens: no crosshair while the player aims at a screen (its cursor is drawn instead)
bool s_bCrosshairHidden = false;

// main-view parameters of the last V_CalcRefdef (crosshair position)
bool s_bHaveRefParams = false;
int s_iViewport[4] = { 0, 0, 0, 0 };
float s_vecViewAngles[3];
float s_vecCrosshairAngle[3];
float s_vecViewOrg[3];
int s_iViewEntity = 0;

cvar_t* s_pCrosshair = nullptr;

int RoundToInt(float f)
{
	return (int)floorf(f + 0.5f);
}

int ToScreenX(float x)
{
	return RoundToInt(s_flOffsetX + s_flScale * x);
}

int ToScreenY(float y)
{
	return RoundToInt(s_flOffsetY + s_flScale * y);
}

bool SetupSprite(HSPRITE hPic, int frame, int rendermode, int r, int g, int b)
{
	if (hPic <= 0)
		return false;

	const model_s* pModel = gEngfuncs.GetSpritePointer(hPic);
	if (!pModel)
		return false;

	if (!gEngfuncs.pTriAPI->SpriteTexture((model_s*)pModel, frame))
		return false;

	gEngfuncs.pTriAPI->RenderMode(rendermode);
	gEngfuncs.pTriAPI->Color4ub((unsigned char)r, (unsigned char)g, (unsigned char)b, 255);
	return true;
}

void RestoreRenderState()
{
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
	gEngfuncs.pTriAPI->Color4ub(255, 255, 255, 255);
}

// Draws the texture coordinates s0,t0 - s1,t1 (0..1) of frame into the screen rectangle x0,y0 - x1,y1 (pixels).
void DrawTexQuad(HSPRITE hPic, int frame, float s0, float t0, float s1, float t1, int x0, int y0, int x1, int y1, int rendermode, int r, int g, int b)
{
	if (!SetupSprite(hPic, frame, rendermode, r, g, b))
		return;

	triangleapi_s* tri = gEngfuncs.pTriAPI;
	tri->Begin(TRI_QUADS);
	tri->TexCoord2f(s0, t0);
	tri->Vertex3f((float)x0, (float)y0, 0.0f);
	tri->TexCoord2f(s1, t0);
	tri->Vertex3f((float)x1, (float)y0, 0.0f);
	tri->TexCoord2f(s1, t1);
	tri->Vertex3f((float)x1, (float)y1, 0.0f);
	tri->TexCoord2f(s0, t1);
	tri->Vertex3f((float)x0, (float)y1, 0.0f);
	tri->End();

	RestoreRenderState();
}

// Draws the part prc of frame (whole frame when prc is NULL) into the screen rectangle x0,y0 - x1,y1 (pixels).
void DrawSpriteQuad(HSPRITE hPic, int frame, const wrect_t* prc, int x0, int y0, int x1, int y1, int rendermode, int r, int g, int b)
{
	if (x1 <= x0 || y1 <= y0)
		return;

	const int sprWidth = gEngfuncs.pfnSPR_Width(hPic, frame);
	const int sprHeight = gEngfuncs.pfnSPR_Height(hPic, frame);
	if (sprWidth <= 0 || sprHeight <= 0)
		return;

	wrect_t rc = { 0, sprWidth, 0, sprHeight };
	if (prc)
		rc = *prc;

	if (rc.right <= rc.left || rc.bottom <= rc.top)
		return;

	// half a texel inside the rectangle: bilinear filtering doesn't pull in the neighbouring icon of the sheet
	// (or the opposite edge of a texture that wraps)
	const float s0 = (rc.left + 0.5f) / sprWidth;
	const float s1 = (rc.right - 0.5f) / sprWidth;
	const float t0 = (rc.top + 0.5f) / sprHeight;
	const float t1 = (rc.bottom - 0.5f) / sprHeight;

	DrawTexQuad(hPic, frame, s0, t0, s1, t1, x0, y0, x1, y1, rendermode, r, g, b);
}

// x, y, w, h in HUD units
void DrawScaled(int frame, int x, int y, int w, int h, const wrect_t* prc, int rendermode)
{
	DrawSpriteQuad(s_hPic, frame, prc, ToScreenX(x), ToScreenY(y), ToScreenX(x + w), ToScreenY(y + h), rendermode, s_iPicR, s_iPicG, s_iPicB);
}

void DrawScaledRect(int frame, int x, int y, const wrect_t* prc, int rendermode)
{
	int w, h;
	if (prc)
	{
		w = prc->right - prc->left;
		h = prc->bottom - prc->top;
	}
	else
	{
		w = gEngfuncs.pfnSPR_Width(s_hPic, frame);
		h = gEngfuncs.pfnSPR_Height(s_hPic, frame);
	}

	DrawScaled(frame, x, y, w, h, prc, rendermode);
}
} // namespace

//
// CHud scaling state
//

void CHud::UpdateScalingInfo()
{
	const bool bWasScaling = m_bShouldScale;
	m_bShouldScale = false;

	float scale = 1.0f;
	if (m_pCvarScale && m_pCvarScale->value != 0.0f && HUD_IsHardware())
	{
		scale = min(ScreenWidth / kRefWidth, ScreenHeight / kRefHeight);
		m_bShouldScale = scale > 1.0f;
	}

	if (!m_bShouldScale)
	{
		m_iWidth = ScreenWidth;
		m_iHeight = ScreenHeight;
		s_flScale = 1.0f;
		s_flOffsetX = s_flOffsetY = 0.0f;
	}
	else
	{
		// the screen in HUD units: >= 640x480, the real aspect ratio
		m_iWidth = (int)(ScreenWidth / scale);
		m_iHeight = (int)(ScreenHeight / scale);
		s_flScale = scale;
		s_flOffsetX = (ScreenWidth - m_iWidth * scale) / 2.0f;
		s_flOffsetY = (ScreenHeight - m_iHeight * scale) / 2.0f;
	}

	if (!s_pCrosshair)
		s_pCrosshair = gEngfuncs.pfnGetCvarPointer("crosshair");

	// hud_scale changed: hand the current crosshair to the other renderer (engine or CL_DrawCrosshair)
	if (bWasScaling != m_bShouldScale)
		SetCrosshair(s_hCrosshair, s_rcCrosshair, s_iCrosshairR, s_iCrosshairG, s_iCrosshairB);
}

bool HUD_IsHardware()
{
	return IEngineStudio.IsHardware && IEngineStudio.IsHardware();
}

float HUD_GetScale()
{
	return s_flScale;
}

int HUD_ToScreenX(int x)
{
	return ToScreenX((float)x);
}

int HUD_ToScreenY(int y)
{
	return ToScreenY((float)y);
}

int HUD_ToHudX(int x)
{
	return RoundToInt((x - s_flOffsetX) / s_flScale);
}

int HUD_ToHudY(int y)
{
	return RoundToInt((y - s_flOffsetY) / s_flScale);
}

int HUD_ToHudSize(int size)
{
	// round up: text metrics must not shrink (lines would overlap)
	return (int)ceilf(size / s_flScale - 0.01f);
}

//
// cl_util.h drawing functions (were macros for the engine functions)
//

void SPR_Set(HSPRITE hPic, int r, int g, int b)
{
	if (!gHUD.ShouldScale())
		gEngfuncs.pfnSPR_Set(hPic, r, g, b);

	s_hPic = hPic;
	s_iPicR = r;
	s_iPicG = g;
	s_iPicB = b;
}

void SPR_Draw(int frame, int x, int y, const wrect_t* prc)
{
	if (!gHUD.ShouldScale())
	{
		gEngfuncs.pfnSPR_Draw(frame, x, y, prc);
		return;
	}

	DrawScaledRect(frame, x, y, prc, kRenderNormal);
}

void SPR_DrawHoles(int frame, int x, int y, const wrect_t* prc)
{
	if (!gHUD.ShouldScale())
	{
		gEngfuncs.pfnSPR_DrawHoles(frame, x, y, prc);
		return;
	}

	DrawScaledRect(frame, x, y, prc, kRenderTransAlpha);
}

void SPR_DrawAdditive(int frame, int x, int y, const wrect_t* prc)
{
	if (!gHUD.ShouldScale())
	{
		gEngfuncs.pfnSPR_DrawAdditive(frame, x, y, prc);
		return;
	}

	DrawScaledRect(frame, x, y, prc, kRenderTransAdd);
}

// SBO: the sniper scope. Draws the texels s0,t0 - s1,t1 of the current sprite's frame (fractions of a texel
// allowed; s0 > s1 or t0 > t1 mirrors) into the screen rectangle x0,y0 - x1,y1 (pixels), with or without hud_scale.
// The texel edges go on the edges of the rectangle, no inset: the caller's texels around them continue the picture.
// Hardware renderer only (the engine can't stretch).
void SPR_DrawTexels(int frame, int x0, int y0, int x1, int y1, float s0, float t0, float s1, float t1, bool bHoles)
{
	if (!HUD_IsHardware() || x1 <= x0 || y1 <= y0 || s_hPic <= 0)
		return;

	const int sprWidth = gEngfuncs.pfnSPR_Width(s_hPic, frame);
	const int sprHeight = gEngfuncs.pfnSPR_Height(s_hPic, frame);
	if (sprWidth <= 0 || sprHeight <= 0)
		return;

	DrawTexQuad(s_hPic, frame, s0 / sprWidth, t0 / sprHeight, s1 / sprWidth, t1 / sprHeight, x0, y0, x1, y1,
		bHoles ? kRenderTransAlpha : kRenderNormal, s_iPicR, s_iPicG, s_iPicB);
}

void FillRGBA(int x, int y, int width, int height, int r, int g, int b, int a)
{
	if (!gHUD.ShouldScale())
	{
		gEngfuncs.pfnFillRGBA(x, y, width, height, r, g, b, a);
		return;
	}

	const int x0 = ToScreenX(x);
	const int y0 = ToScreenY(y);
	gEngfuncs.pfnFillRGBA(x0, y0, ToScreenX(x + width) - x0, ToScreenY(y + height) - y0, r, g, b, a);
}

void SetCrosshair(HSPRITE hspr, wrect_t rc, int r, int g, int b)
{
	if (!gHUD.ShouldScale() && !s_bCrosshairHidden)
	{
		gEngfuncs.pfnSetCrosshair(hspr, rc, r, g, b);
	}
	else
	{
		// drawn by CL_DrawCrosshair, or hidden
		wrect_t nullrc = { 0, 0, 0, 0 };
		gEngfuncs.pfnSetCrosshair(0, nullrc, 0, 0, 0);
	}

	s_hCrosshair = hspr;
	s_rcCrosshair = rc;
	s_iCrosshairR = r;
	s_iCrosshairG = g;
	s_iCrosshairB = b;
}

// SBO 2.0 world screens (worldscreen.cpp WorldScreen_Frame, every frame): the crosshair hidden or shown again
void HUD_SetCrosshairHidden(bool hidden)
{
	if (hidden == s_bCrosshairHidden)
		return;
	s_bCrosshairHidden = hidden;
	SetCrosshair(s_hCrosshair, s_rcCrosshair, s_iCrosshairR, s_iCrosshairG, s_iCrosshairB);
}

int DrawConsoleString(int x, int y, const char* string)
{
	if (!gHUD.ShouldScale())
		return gEngfuncs.pfnDrawConsoleString(x, y, (char*)string);

	// native glyph size at the scaled position; returns the end position in HUD units
	return HUD_ToHudX(gEngfuncs.pfnDrawConsoleString(ToScreenX(x), ToScreenY(y), (char*)string));
}

void GetConsoleStringSize(const char* string, int* width, int* height)
{
	gEngfuncs.pfnDrawConsoleStringLen(string, width, height);

	if (gHUD.ShouldScale())
	{
		*width = HUD_ToHudSize(*width);
		*height = HUD_ToHudSize(*height);
	}
}

//
// Crosshair
//

void HUD_SetRefParams(const ref_params_s* pparams)
{
	for (int i = 0; i < 4; i++)
		s_iViewport[i] = pparams->viewport[i];

	for (int i = 0; i < 3; i++)
	{
		s_vecViewAngles[i] = pparams->viewangles[i];
		s_vecCrosshairAngle[i] = pparams->crosshairangle[i];
		s_vecViewOrg[i] = pparams->vieworg[i];
	}

	s_iViewEntity = pparams->viewentity;
	s_bHaveRefParams = true;
}

void CL_DrawCrosshair(int intermission)
{
	// with hud_scale 0 (or software) the engine draws the crosshair
	if (!gHUD.ShouldScale() || !s_bHaveRefParams || intermission || s_bCrosshairHidden)
		return;

	if (!s_pCrosshair || s_pCrosshair->value == 0.0f)
		return;

	if (s_hCrosshair <= 0)
		return;

	if (g_iUser1)
	{
		// spectators: hud_spectator.cpp sets a crosshair for the first-person and free-look modes only
		if (g_iUser1 != OBS_IN_EYE && g_iUser1 != OBS_ROAMING)
			return;
	}
	else
	{
		// don't draw if the player is dead or looking through a camera (trigger_camera)
		cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
		if (gHUD.m_fPlayerDead || !pLocal || s_iViewEntity != pLocal->index)
			return;
	}

	const int width = s_rcCrosshair.right - s_rcCrosshair.left;
	const int height = s_rcCrosshair.bottom - s_rcCrosshair.top;
	if (width <= 0 || height <= 0)
		return;

	// centre of the main view
	float cx = s_iViewport[0] + s_iViewport[2] / 2.0f;
	float cy = s_iViewport[1] + s_iViewport[3] / 2.0f;

	// auto-aim: the server moves the crosshair with crosshairangle
	if (s_vecCrosshairAngle[0] != 0.0f || s_vecCrosshairAngle[1] != 0.0f)
	{
		vec3_t angles, forward, world, screen;
		for (int i = 0; i < 3; i++)
			angles[i] = s_vecViewAngles[i] + s_vecCrosshairAngle[i];

		AngleVectors(angles, forward, NULL, NULL);

		for (int i = 0; i < 3; i++)
			world[i] = s_vecViewOrg[i] + forward[i] * 64.0f;

		if (!gEngfuncs.pTriAPI->WorldToScreen(world, screen))
		{
			// screen is -1..1 with y up (hud_spectator.cpp's XPROJECT/YPROJECT)
			cx = s_iViewport[0] + (1.0f + screen[0]) * s_iViewport[2] / 2.0f;
			cy = s_iViewport[1] + (1.0f - screen[1]) * s_iViewport[3] / 2.0f;
		}
	}

	const int w = RoundToInt(width * s_flScale);
	const int h = RoundToInt(height * s_flScale);
	const int x0 = RoundToInt(cx - w / 2.0f);
	const int y0 = RoundToInt(cy - h / 2.0f);

	DrawSpriteQuad(s_hCrosshair, 0, &s_rcCrosshair, x0, y0, x0 + w, y0 + h, kRenderTransAlpha, s_iCrosshairR, s_iCrosshairG, s_iCrosshairB);
}
