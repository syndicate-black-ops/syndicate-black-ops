//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: SBO side of the kill feed bridge (sbo_bridge.h, "Kill feed" section): HUD sprites and HUD state for
//          the VGUI2 kill feed (hud_deathnotice.cpp). Kept out of sbo_bridge.cpp so the kill feed and other
//          bridge work don't touch the same file.
//
// SBO world: compiled with SBO's client include paths (SDK 2.3, VGUI1) + sbo_compat.h. No Source SDK headers.
//=================================================================//

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "cvardef.h"
#include "triangleapi.h"
#include "viewport.h"

#include "sbo_bridge.h"

namespace sbo
{
int GetHudSpriteIndex(const char *name)
{
	if (!name || !name[0])
		return -1;
	return gHUD.GetSpriteIndex(name);
}

bool GetHudSpriteSize(int spriteIndex, int &wide, int &tall)
{
	wide = tall = 0;
	if (spriteIndex < 0 || !gHUD.GetSprite(spriteIndex))
		return false;

	const wrect_t &rc = gHUD.GetSpriteRect(spriteIndex);
	wide = rc.right - rc.left;
	tall = rc.bottom - rc.top;
	return wide > 0 && tall > 0;
}

// BugfixedHL-Rebased CHudRenderer::DrawSprite (SpriteDrawMode::Additive), itself based on Xash3D's.
void DrawHudSpriteAdditive(int spriteIndex, int x, int y, int r, int g, int b)
{
	if (spriteIndex < 0)
		return;

	HSPRITE hSpr = gHUD.GetSprite(spriteIndex);
	if (hSpr <= 0)
		return;

	const struct model_s *pModel = gEngfuncs.GetSpritePointer(hSpr);
	if (!pModel)
		return;

	const int frame = 0;
	float width = (float)gEngfuncs.pfnSPR_Width(hSpr, frame);
	float height = (float)gEngfuncs.pfnSPR_Height(hSpr, frame);
	if (width <= 0 || height <= 0)
		return;

	wrect_t rc = gHUD.GetSpriteRect(spriteIndex);

	// "some stupid modmakers set wrong rectangles in hud.txt" (BHL)
	if (rc.left <= 0 || rc.left >= width)
		rc.left = 0;
	if (rc.top <= 0 || rc.top >= height)
		rc.top = 0;
	if (rc.right <= 0 || rc.right > width)
		rc.right = (int)width;
	if (rc.bottom <= 0 || rc.bottom > height)
		rc.bottom = (int)height;

	const float s0 = rc.left / width;
	const float t0 = rc.top / height;
	const float s1 = rc.right / width;
	const float t1 = rc.bottom / height;
	const float w = (float)(rc.right - rc.left);
	const float h = (float)(rc.bottom - rc.top);
	const float fx = (float)x;
	const float fy = (float)y;

	triangleapi_s *tri = gEngfuncs.pTriAPI;
	tri->Color4ub((unsigned char)r, (unsigned char)g, (unsigned char)b, 255);
	tri->SpriteTexture((struct model_s *)pModel, frame);
	tri->RenderMode(kRenderTransAdd);

	tri->Begin(TRI_QUADS);
	tri->TexCoord2f(s0, t0);
	tri->Vertex3f(fx, fy, 0);
	tri->TexCoord2f(s1, t0);
	tri->Vertex3f(fx + w, fy, 0);
	tri->TexCoord2f(s1, t1);
	tri->Vertex3f(fx + w, fy + h, 0);
	tri->TexCoord2f(s0, t1);
	tri->Vertex3f(fx, fy + h, 0);
	tri->End();

	tri->RenderMode(kRenderNormal);
}

bool KillFeedShouldDraw()
{
	if (!gHUD.m_pCvarDraw || !gHUD.m_pCvarDraw->value)
		return false;
	if (gHUD.m_iHideHUDDisplay & HIDEHUD_ALL)
		return false;
	if (gHUD.m_iIntermission)
		return false;

	const char *level = gEngfuncs.pfnGetLevelName();
	if (!level || !level[0])
		return false; // not connected (main menu): don't leave rows on screen

	if (gViewPort && !gViewPort->AllowedToPrintText())
		return false;

	return true;
}

CvarHandle FindCvar(const char *name)
{
	return reinterpret_cast<CvarHandle>(gEngfuncs.pfnGetCvarPointer((char *)name));
}
} // namespace sbo
