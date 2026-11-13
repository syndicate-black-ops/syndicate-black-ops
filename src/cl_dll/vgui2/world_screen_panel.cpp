//========= SBO world-space VGUI2 screens =========//
//
// Purpose: The VGUI2 side of the world screens (see worldscreen/worldscreen.h and docs/WORLD_SCREENS.md).
//
// CWorldScreenContent: one screen's panel, an EditablePanel named "WorldScreen" built from the layout
//   ui/scripts/screens/<panelname>.res (LoadControlSettings: Labels, Buttons, ImagePanels, ... of the SDK's
//   vgui_controls). The layout's "WorldScreen" section sets its pixel size ("wide", "tall"; default 512 x 256,
//   at most WORLDSCREEN_MAX_PIXELS and the VGUI2 screen size). Scheme: ui/resource/worldscreenscheme.res (fixed-size
//   fonts, the button colors). A button's "command" goes to the server (worldscreen::SendCommand).
// CWorldScreenHost: one per screen slot, a child of CClientViewport at (0,0) at the panel's pixel size, painting
//   nothing itself. Its PaintTraverse override is the trick: it is called by the engine's VGUI2 paint pass (the only
//   time the engine surface is set up for drawing), and instead of painting its children onto the screen it binds
//   the slot's FBO (wsgl::BeginRender), lets VGUI2 paint them there (BaseClass::PaintTraverse; first the host's
//   opaque black background, which clears the texture), and puts the engine's framebuffer and viewport back
//   (wsgl::EndRender). Only when a 3D view of this frame saw the screen (worldscreen::ConsumeSeen), at
//   worldscreen_rate Hz or at once after a change (hover, click, control message), and within the frame's repaint
//   budget (worldscreen::ClaimRepaint: the screens that waited longest first).
// CWorldScreenCursor: the cursor at the center of the view while the player aims at a screen (SBO Source's
//   VguiScreenCursor HUD element; not drawn into the texture). Image: ui/gfx/vgui/worldscreen_cursor.tga (SBO's own
//   arrow, its tip at the image's top-left corner = the center of the view), a cross without it.
//
// Interaction, as SBO Source does it: no VGUI2 mouse messages (the real mouse never reaches these panels: mouse
// input is off on all of them). The topmost visible, enabled Button under the aimed pixel is armed (hover color);
// a left press depresses it (ForceDepressed), the release over the same button clicks it (DoClick: its action
// signal "Command" -> CWorldScreenContent::OnCommand). A right press does nothing on a button.
//
// Values: a text change (WSCREEN_CONTROL_TEXT) reaching a ProgressBar / ContinuousProgressBar, AnalogBar /
// ContinuousAnalogBar or Slider sets its value: the text as a percentage, 0 .. 100 (clamped).
//
// Animations (Source's hudanimations format, the SDK's vgui2::AnimationController): each screen's content panel
// has its own controller, loaded from ui/scripts/screens/<panelname>_anims.txt if the file exists. A sequence starts
// from a button command "anim:<sequence>" (this player only, no server round trip; a command may list several
// commands separated by ',', the others go to the server), or from the map (WSCREEN_CONTROL_ANIMATION, everybody).
// A script's FireCommand may start sequences ("anim:..."); other commands from a script are not sent to the server
// (every client runs the script: the server would get them once per player). While a screen animates it is
// repainted every frame (within the frame's repaint budget), then at worldscreen_rate again.
//
// The render target's texture name is allocated by the engine's VGUI2 surface (CreateNewTextureID +
// DrawSetTextureRGBA, then read back from GL_TEXTURE_BINDING_2D), so it can't collide with the engine's own texture
// numbering; if that doesn't yield a name, wsgl falls back to glGenTextures.
//
// SDK world: compiled with hl1_source_sdk include paths; reaches SBO only through worldscreen.h.
//===============================================================//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FileSystem.h>
#include <KeyValues.h>
#include <tier2/tier2.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui_controls/AnalogBar.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/Slider.h>

#include "client_vgui.h"
#include "client_viewport.h"
#include "../../game_shared/vgui_screen_shared.h"
#include "worldscreen.h"
#include "worldscreen_gl.h"

namespace
{
const char *const WORLDSCREEN_SCHEME_FILE = VGUI2_ROOT_DIR "resource/worldscreenscheme.res";
const char *const WORLDSCREEN_LAYOUT_DIR = VGUI2_ROOT_DIR "scripts/screens/";
const char *const CURSOR_FILE = VGUI2_ROOT_DIR "gfx/vgui/worldscreen_cursor";

vgui2::HScheme s_hScheme = 0;

// Mouse input off on a panel and all its children: the real cursor (scoreboard, chat) must never reach these
// invisible panels at the top-left of the viewport. ipanel(), unlike Panel::SetMouseInputEnabled, doesn't make the
// surface recalculate the cursor.
void DisableMouseInput(vgui2::VPANEL panel)
{
	vgui2::ipanel()->SetMouseInputEnabled(panel, false);
	for (int i = 0; i < vgui2::ipanel()->GetChildCount(panel); i++)
		DisableMouseInput(vgui2::ipanel()->GetChild(panel, i));
}

// The topmost visible, enabled Button under (x, y) (local to parent), searched last child first; NULL if none.
vgui2::Button *FindButtonAt(vgui2::Panel *parent, int x, int y)
{
	for (int i = parent->GetChildCount() - 1; i >= 0; i--)
	{
		vgui2::Panel *child = parent->GetChild(i);
		if (!child || !child->IsVisible())
			continue;
		int cx, cy, cw, ct;
		child->GetBounds(cx, cy, cw, ct);
		if (x < cx || y < cy || x >= cx + cw || y >= cy + ct)
			continue;
		if (vgui2::Button *inner = FindButtonAt(child, x - cx, y - cy))
			return inner;
		vgui2::Button *button = dynamic_cast<vgui2::Button *>(child);
		if (button && button->IsEnabled())
			return button;
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
// One screen's panel
//-----------------------------------------------------------------------------
class CWorldScreenContent : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CWorldScreenContent, vgui2::EditablePanel);

public:
	CWorldScreenContent(vgui2::Panel *pParent, int slot)
	    : BaseClass(pParent, "WorldScreen"), m_iSlot(slot)
	{
		SetProportional(false);
		SetBounds(0, 0, WORLDSCREEN_DEFAULT_WIDE, WORLDSCREEN_DEFAULT_TALL);
		SetKeyBoardInputEnabled(false);
		SetMouseInputEnabled(false);
		if (s_hScheme)
			SetScheme(s_hScheme);
	}

	// A button's command (or a script's FireCommand): ','-separated commands; "anim:<sequence>" runs here, the
	// others go to the server (not from a script, see the file comment).
	void OnCommand(const char *command) override
	{
		if (!command || !strcmp(command, "vguicancel"))
			return;
		char buf[256];
		strncpy(buf, command, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = 0;
		for (char *part = strtok(buf, ","); part; part = strtok(nullptr, ","))
		{
			while (*part == ' ')
				part++;
			for (char *end = part + strlen(part); end > part && end[-1] == ' ';)
				*--end = 0;
			if (!*part)
				continue;
			const size_t prefix = strlen(WSCREEN_LOCAL_ANIM_PREFIX);
			if (!strncmp(part, WSCREEN_LOCAL_ANIM_PREFIX, prefix))
				StartSequence(part + prefix);
			else if (m_bInAnimUpdate)
				worldscreen::DebugPrintf("worldscreen: screen %d animation script FireCommand \"%s\" ignored (only "
				                    "\"" WSCREEN_LOCAL_ANIM_PREFIX "<sequence>\" runs from a script)\n", m_iSlot, part);
			else
				worldscreen::SendCommand(m_iSlot, part);
		}
	}

	// The layout's animation script, if there is one. Call after the panel has its final size.
	void LoadAnimations(const char *panelName)
	{
		char path[160];
		snprintf(path, sizeof(path), "%s%s_anims.txt", WORLDSCREEN_LAYOUT_DIR, panelName);
		if (!g_pFullFileSystem || !g_pFullFileSystem->FileExists(path))
			return;
		m_pAnim = new vgui2::AnimationController(this);
		m_pAnim->SetProportional(false);
		if (!m_pAnim->SetScriptFile(GetVPanel(), path, true))
			worldscreen::DebugPrintf("worldscreen: screen %d: %s could not be read\n", m_iSlot, path);
		else if (worldscreen::DebugLevel() > 0)
			worldscreen::Printf("worldscreen: screen %d: animations %s\n", m_iSlot, path);
	}

	bool StartSequence(const char *name)
	{
		if (!m_pAnim || !m_pAnim->StartAnimationSequence(this, name))
		{
			worldscreen::DebugPrintf("worldscreen: screen %d: no animation sequence \"%s\"%s\n", m_iSlot, name,
			    m_pAnim ? "" : " (the layout has no _anims.txt)");
			return false;
		}
		const double until = vgui2::system()->GetCurrentTime() + m_pAnim->GetAnimationSequenceLength(name) + 0.1;
		if (until > m_flAnimatingUntil)
			m_flAnimatingUntil = until;
		m_bChanged = true;
		return true;
	}

	// Once a frame: advances the animations. True while something animates (repaint every frame).
	bool UpdateAnimations(double now)
	{
		if (!m_pAnim)
			return false;
		m_bInAnimUpdate = true;
		m_pAnim->UpdateAnimations((float)now);
		m_bInAnimUpdate = false;
		return m_pAnim->GetNumActiveAnimations() > 0 || now < m_flAnimatingUntil;
	}

	// Something changed outside a repaint (a local animation started): true once.
	bool ConsumeChanged()
	{
		const bool changed = m_bChanged;
		m_bChanged = false;
		return changed;
	}

private:
	int m_iSlot;
	vgui2::AnimationController *m_pAnim = nullptr;
	bool m_bInAnimUpdate = false;
	bool m_bChanged = false;
	double m_flAnimatingUntil = 0;
};

//-----------------------------------------------------------------------------
// Render host, one per screen slot
//-----------------------------------------------------------------------------
class CWorldScreenHost : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CWorldScreenHost, vgui2::Panel);

public:
	CWorldScreenHost(vgui2::Panel *pParent, int slot)
	    : BaseClass(pParent, "SboWorldScreenHost"), m_iSlot(slot)
	{
		SetProportional(false);
		SetBounds(0, 0, WORLDSCREEN_DEFAULT_WIDE, WORLDSCREEN_DEFAULT_TALL);
		SetKeyBoardInputEnabled(false);
		SetMouseInputEnabled(false);
		SetPaintEnabled(false);
		SetPaintBackgroundEnabled(true); // clears the texture, see PaintBackground
		SetPaintBorderEnabled(false);
		if (s_hScheme)
			SetScheme(s_hScheme);
	}

	// Builds the panel from the layout (or from layoutText, the world screen editor's preview). False: no layout
	// file (no layout in the text).
	bool Load(const char *panelName, int &wide, int &tall, const char *layoutText = nullptr)
	{
		char path[160];
		snprintf(path, sizeof(path), "%s%s.res", WORLDSCREEN_LAYOUT_DIR, panelName);
		KeyValues *layout = nullptr;
		if (layoutText && *layoutText)
		{
			layout = new KeyValues(path);
			if (!layout->LoadFromBuffer(path, layoutText) || !layout->GetFirstSubKey())
			{
				layout->deleteThis();
				return false;
			}
		}
		else if (!g_pFullFileSystem || !g_pFullFileSystem->FileExists(path))
		{
			Unload();
			return false;
		}

		Unload();
		m_pContent = new CWorldScreenContent(this, m_iSlot);
		m_pContent->LoadControlSettings(path, nullptr, layout);
		if (layout)
			layout->deleteThis();
		m_pContent->GetSize(wide, tall);

		int sw = 0, st = 0;
		vgui2::surface()->GetScreenSize(sw, st);
		const int maxWide = (sw > 0 && sw < WORLDSCREEN_MAX_PIXELS) ? sw : WORLDSCREEN_MAX_PIXELS;
		const int maxTall = (st > 0 && st < WORLDSCREEN_MAX_PIXELS) ? st : WORLDSCREEN_MAX_PIXELS;
		if (wide < 16 || tall < 16 || wide > maxWide || tall > maxTall)
		{
			worldscreen::DebugPrintf("worldscreen: %s: \"WorldScreen\" size %d x %d out of range (16 .. %d x %d), using "
			                    "%d x %d\n", path, wide, tall, maxWide, maxTall, wide < 16 || wide > maxWide ?
			                    WORLDSCREEN_DEFAULT_WIDE : wide, tall < 16 || tall > maxTall ? WORLDSCREEN_DEFAULT_TALL : tall);
			if (wide < 16 || wide > maxWide)
				wide = WORLDSCREEN_DEFAULT_WIDE;
			if (tall < 16 || tall > maxTall)
				tall = WORLDSCREEN_DEFAULT_TALL;
		}
		m_iWide = wide;
		m_iTall = tall;
		SetBounds(0, 0, wide, tall);
		m_pContent->SetBounds(0, 0, wide, tall);
		DisableMouseInput(m_pContent->GetVPanel());
		m_pContent->LoadAnimations(panelName);
		m_bForceRender = true;
		m_flLastRender = -1.0;
		return true;
	}

	void Unload()
	{
		SetHover(nullptr);
		m_hPressed = nullptr;
		if (m_pContent)
		{
			m_pContent->MarkForDeletion();
			m_pContent = nullptr;
		}
	}

	int RenderCount() const
	{
		return m_iRenderCount;
	}

	void OnThink() override
	{
		BaseClass::OnThink();
		// stay at the top-left corner at the panel's size (the FBO mapping relies on screen pixel == texel); the
		// layout's own position is not used
		if (m_pContent)
		{
			int x, y, w, t;
			GetBounds(x, y, w, t);
			if (x != 0 || y != 0 || w != m_iWide || t != m_iTall)
				SetBounds(0, 0, m_iWide, m_iTall);
			m_pContent->GetBounds(x, y, w, t);
			if (x != 0 || y != 0 || w != m_iWide || t != m_iTall)
				m_pContent->SetBounds(0, 0, m_iWide, m_iTall);
		}
	}

	void PaintBackground() override
	{
		vgui2::surface()->DrawSetColor(0, 0, 0, 255);
		vgui2::surface()->DrawFilledRect(0, 0, GetWide(), GetTall());
	}

	void PaintTraverse(bool /*repaint*/, bool allowForce) override
	{
		// Never paint onto the screen: only into the FBO, below. Only when a 3D view of this frame saw the screen.
		const bool seen = worldscreen::ConsumeSeen(m_iSlot);
		const double now = vgui2::system()->GetCurrentTime();
		// animations run whether the screen is seen or not (they are timed); while one runs, every frame is due
		const bool animating = m_pContent && m_pContent->UpdateAnimations(now);
		if (m_pContent && m_pContent->ConsumeChanged())
			m_bForceRender = true;
		if (!m_pContent || !IsVisible() || !seen || !wsgl::Init())
			return;

		const bool due = m_bForceRender || animating || !wsgl::HasContent(m_iSlot) || m_flLastRender < 0 || now < m_flLastRender ||
		    now - m_flLastRender >= 1.0 / worldscreen::UpdateRate();
		if (!due)
			return;

		if (!wsgl::HasTarget(m_iSlot, m_iWide, m_iTall))
		{
			if (m_flLastCreateTry >= 0 && now >= m_flLastCreateTry && now - m_flLastCreateTry < 1.0)
				return;
			m_flLastCreateTry = now;
			if (!CreateTarget())
				return;
		}

		if (!worldscreen::ClaimRepaint(m_iSlot, wsgl::HasContent(m_iSlot), m_flLastRender))
			return; // the frame's budget is used up or kept for screens that waited longer: a later frame

		int sw = 0, st = 0;
		vgui2::surface()->GetScreenSize(sw, st);
		vgui2::surface()->DrawFlushText(); // whatever the engine batched so far belongs on the screen

		if (!wsgl::BeginRender(m_iSlot, sw, st))
			return;
		BaseClass::PaintTraverse(true, allowForce);
		vgui2::surface()->DrawFlushText(); // our text belongs in the texture
		wsgl::EndRender();

		m_flLastRender = now;
		m_bForceRender = false;
		m_iRenderCount++;
		if (worldscreen::DebugLevel() > 1 || (worldscreen::DebugLevel() > 0 && m_iRenderCount == 1))
			worldscreen::Printf("worldscreen: screen %d render #%d (%dx%d, vgui2 screen %dx%d)\n", m_iSlot,
			    m_iRenderCount, m_iWide, m_iTall, sw, st);
	}

	// Aim update: the button under the pixel is armed (hover color)
	void SetCursor(bool onScreen, int x, int y)
	{
		SetHover(onScreen && m_pContent ? FindButtonAt(m_pContent, x, y) : nullptr);
	}

	void MouseButton(int button, bool down)
	{
		if (button != 0)
			return; // right click: nothing on a button (the attack is still swallowed)
		vgui2::Button *hover = dynamic_cast<vgui2::Button *>(m_hHover.Get());
		if (down)
		{
			m_hPressed = hover;
			if (hover)
			{
				hover->ForceDepressed(true);
				m_bForceRender = true;
			}
			return;
		}

		vgui2::Button *pressed = dynamic_cast<vgui2::Button *>(m_hPressed.Get());
		m_hPressed = nullptr;
		if (!pressed)
			return;
		pressed->ForceDepressed(false);
		if (pressed == hover && pressed->IsEnabled() && pressed->IsVisible())
		{
			if (worldscreen::DebugLevel() > 0)
				worldscreen::Printf("worldscreen: screen %d button \"%s\" clicked\n", m_iSlot, pressed->GetName());
			pressed->DoClick(); // action signal "Command" -> CWorldScreenContent::OnCommand
			pressed->SetArmed(true); // still hovered
		}
		m_bForceRender = true;
	}

	void Control(int op, const char *control, const char *value)
	{
		if (!m_pContent)
			return;
		if (op == WSCREEN_CONTROL_ANIMATION)
		{
			m_pContent->StartSequence(value ? value : "");
			m_bForceRender = true;
			return;
		}
		vgui2::Panel *panel = m_pContent->FindChildByName(control, true);
		if (!panel)
		{
			if (worldscreen::DebugLevel() > 0)
				worldscreen::Printf("worldscreen: screen %d has no control \"%s\"\n", m_iSlot, control);
			return;
		}
		const bool on = value && value[0] == '1';
		switch (op)
		{
		case WSCREEN_CONTROL_TEXT:
			SetTextOrValue(panel, value ? value : "");
			break;
		case WSCREEN_CONTROL_ENABLED:
			panel->SetEnabled(on);
			break;
		case WSCREEN_CONTROL_VISIBLE:
			panel->SetVisible(on);
			break;
		default:
			return;
		}
		// a hovered or pressed button that went away is let go
		vgui2::Panel *hover = m_hHover.Get();
		if (hover && (!hover->IsEnabled() || !hover->IsVisible()))
			SetHover(nullptr);
		m_bForceRender = true;
	}

private:
	// A control with a value takes the text as a percentage (0 .. 100, clamped); a Label (Button) its text.
	void SetTextOrValue(vgui2::Panel *panel, const char *text)
	{
		float percent = (float)atof(text);
		percent = percent < 0.0f ? 0.0f : (percent > 100.0f ? 100.0f : percent);
		if (vgui2::ProgressBar *bar = dynamic_cast<vgui2::ProgressBar *>(panel))
			bar->SetProgress(percent / 100.0f);
		else if (vgui2::AnalogBar *analog = dynamic_cast<vgui2::AnalogBar *>(panel))
			analog->SetAnalogValue(percent / 100.0f);
		else if (vgui2::Slider *slider = dynamic_cast<vgui2::Slider *>(panel))
		{
			int lo = 0, hi = 100;
			slider->GetRange(lo, hi);
			slider->SetValue(lo + (int)((hi - lo) * percent / 100.0f + 0.5f), false);
		}
		else if (vgui2::Label *label = dynamic_cast<vgui2::Label *>(panel))
			label->SetText(text);
	}

	void SetHover(vgui2::Button *button)
	{
		vgui2::Button *old = dynamic_cast<vgui2::Button *>(m_hHover.Get());
		if (old == button)
			return;
		if (old)
			old->SetArmed(false);
		m_hHover = button;
		if (button)
			button->SetArmed(true);
		m_bForceRender = true;
		if (worldscreen::DebugLevel() > 0)
			worldscreen::Printf("worldscreen: screen %d hover %s -> %s\n", m_iSlot, old ? old->GetName() : "(none)",
			    button ? button->GetName() : "(none)");
	}

	bool CreateTarget()
	{
		// Let the engine's VGUI2 surface allocate the GL texture name (see the file comment); the same id is used
		// again for a new size or a new GL context
		if (m_iTexId < 0)
			m_iTexId = vgui2::surface()->CreateNewTextureID(true);
		unsigned char *black = new unsigned char[m_iWide * m_iTall * 4]();
		const unsigned int prev = wsgl::GetBoundTexture2D();
		vgui2::surface()->DrawSetTextureRGBA(m_iTexId, black, m_iWide, m_iTall, true, true);
		unsigned int glName = wsgl::GetBoundTexture2D();
		wsgl::BindTexture2D(prev);
		delete[] black;

		if (glName == prev)
			glName = 0; // the surface didn't bind a new texture: fall back to glGenTextures
		if (worldscreen::DebugLevel() > 0)
			worldscreen::Printf("worldscreen: screen %d VGUI2 texture id %d -> GL texture %u%s\n", m_iSlot, m_iTexId,
			    glName, glName ? "" : " (none seen, using glGenTextures)");
		return wsgl::CreateTarget(m_iSlot, glName, m_iWide, m_iTall);
	}

	int m_iSlot;
	CWorldScreenContent *m_pContent = nullptr;
	int m_iWide = WORLDSCREEN_DEFAULT_WIDE;
	int m_iTall = WORLDSCREEN_DEFAULT_TALL;
	int m_iTexId = -1;
	int m_iRenderCount = 0;
	bool m_bForceRender = false;
	double m_flLastRender = -1.0;
	double m_flLastCreateTry = -1.0;
	vgui2::PHandle m_hHover;   // the armed button
	vgui2::PHandle m_hPressed; // the button that got the left press
};

//-----------------------------------------------------------------------------
// The cursor at the center of the view
//-----------------------------------------------------------------------------
class CWorldScreenCursor : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CWorldScreenCursor, vgui2::Panel);

public:
	CWorldScreenCursor(vgui2::Panel *pParent)
	    : BaseClass(pParent, "WorldScreenCursor")
	{
		SetProportional(false);
		SetKeyBoardInputEnabled(false);
		SetMouseInputEnabled(false);
		SetPaintBackgroundEnabled(false);
		SetPaintBorderEnabled(false);
		SetVisible(false);
	}

	void Show(bool show)
	{
		if (show == IsVisible())
			return;
		if (show)
		{
			// 32 pixels at 1080 lines, the tip (the image's top-left corner) at the center of the screen
			int sw = 0, st = 0;
			vgui2::surface()->GetScreenSize(sw, st);
			int size = st * 32 / 1080;
			if (size < 16)
				size = 16;
			SetBounds(sw / 2, st / 2, size, size);
			MoveToFront();
		}
		SetVisible(show);
	}

	void Paint() override
	{
		if (m_iTexId < 0)
		{
			m_iTexId = vgui2::surface()->CreateNewTextureID();
			vgui2::surface()->DrawSetTextureFile(m_iTexId, CURSOR_FILE, true, false);
			int w = 0, t = 0;
			vgui2::surface()->DrawGetTextureSize(m_iTexId, w, t);
			m_bHaveImage = w > 0 && t > 0;
			if (!m_bHaveImage)
				worldscreen::DebugPrintf("worldscreen: %s.tga not found: a cross is drawn instead\n", CURSOR_FILE);
		}

		const int w = GetWide(), t = GetTall();
		if (m_bHaveImage)
		{
			vgui2::surface()->DrawSetTexture(m_iTexId);
			vgui2::surface()->DrawSetColor(255, 255, 255, 255);
			vgui2::surface()->DrawTexturedRect(0, 0, w, t);
			return;
		}
		// image missing: a small cross on the tip
		vgui2::surface()->DrawSetColor(82, 255, 132, 255);
		vgui2::surface()->DrawFilledRect(0, 0, w / 3, 2);
		vgui2::surface()->DrawFilledRect(0, 0, 2, t / 3);
	}

private:
	int m_iTexId = -1;
	bool m_bHaveImage = false;
};

CWorldScreenHost *s_pHosts[WORLDSCREEN_MAX] = {};
CWorldScreenCursor *s_pCursor = nullptr;

CWorldScreenHost *Host(int slot)
{
	return (slot >= 0 && slot < WORLDSCREEN_MAX) ? s_pHosts[slot] : nullptr;
}
} // namespace

bool VGui2_WorldScreen_Init()
{
	if (s_pCursor)
		return true;
	if (!g_pViewport)
		return false;
	s_hScheme = vgui2::scheme()->LoadSchemeFromFile(WORLDSCREEN_SCHEME_FILE, "WorldScreenScheme");
	s_pCursor = new CWorldScreenCursor(g_pViewport);
	return true;
}

bool VGui2_WorldScreen_Create(int slot, const char *panelName, int &pixelWide, int &pixelTall)
{
	if (slot < 0 || slot >= WORLDSCREEN_MAX || !g_pViewport)
		return false;
	if (!s_pHosts[slot])
		s_pHosts[slot] = new CWorldScreenHost(g_pViewport, slot);
	return s_pHosts[slot]->Load(panelName, pixelWide, pixelTall);
}

bool VGui2_WorldScreen_CreateFromText(int slot, const char *panelName, const char *layoutText, int &pixelWide,
    int &pixelTall)
{
	if (slot < 0 || slot >= WORLDSCREEN_MAX || !g_pViewport)
		return false;
	if (!s_pHosts[slot])
		s_pHosts[slot] = new CWorldScreenHost(g_pViewport, slot);
	return s_pHosts[slot]->Load(panelName, pixelWide, pixelTall, layoutText);
}

void VGui2_WorldScreen_Remove(int slot)
{
	if (CWorldScreenHost *host = Host(slot))
		host->Unload();
}

int VGui2_WorldScreen_RenderCount(int slot)
{
	CWorldScreenHost *host = Host(slot);
	return host ? host->RenderCount() : 0;
}

void VGui2_WorldScreen_SetCursor(int slot, bool onScreen, int x, int y)
{
	if (CWorldScreenHost *host = Host(slot))
		host->SetCursor(onScreen, x, y);
}

void VGui2_WorldScreen_MouseButton(int slot, int button, bool down)
{
	if (CWorldScreenHost *host = Host(slot))
		host->MouseButton(button, down);
}

void VGui2_WorldScreen_Control(int slot, int op, const char *control, const char *value)
{
	if (CWorldScreenHost *host = Host(slot))
		host->Control(op, control, value);
}

void VGui2_WorldScreen_ShowCursor(bool show)
{
	if (s_pCursor)
		s_pCursor->Show(show);
}
