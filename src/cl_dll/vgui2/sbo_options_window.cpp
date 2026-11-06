//========= SBO VGUI2: Options window (the new menus) =========//
//
// Purpose: see sbo_options_window.h. What each control writes (from HL25's gameui.so; docs/MENUS.md):
//   Multiplayer  name: `name "x"` console command; player model colors topcolor / bottomcolor (Cvar_SetValue);
//                spray: cl_logofile + cl_logocolor commands and tempdecal.wad built from logos/<name>.bmp with the
//                color (GameUI's UpdateLogoWAD); user.scr rows: GameUI's Multiplayer > Advanced (the script's
//                commands, user.scr rewritten); name color: SBO's namecolor (the chat color picker's value)
//   Keyboard     gfx/shell/kb_act.lst actions, the engine's bindings; Apply: `unbind` for keys taken off, `bind` for
//                every bound action (key and alternate), then `exec userconfig.cfg`; defaults from kb_def.lst
//   Mouse        m_pitch below 0 (reverse), m_filter, sensitivity 0.2..20, m_rawinput, gl_lowlatency, sv_aim
//   Audio        volume 0..2, mp3volume 0..1, sound quality `hisound 1` / `hisound 0`
//   Video        brightness 0..2, gamma 1..3, gl_vsync, gl_widescreen_yfov, r_prefertexturefiltering,
//                gl_use_shaders; resolution / renderer / windowed: `_setvideomode w h 32`, `_setrenderer r windowed|
//                fullscreen`, `_restart` when they or gamma changed (the modes from IGameUIFuncs::GetVideoModes)
//   Voice        voice_modenable, voice_scale; transmit volume and mic boost through the engine's voice tweak
//                interface; microphone test (voice tweak mode with the level meter)
//   Content      r_detailtextures; original models `_sethdmodels`, addons folder `_setaddons_folder`, then
//                `_restart`; the content lock (GameUI's password store, violence_* 0 / 1)
//   Chat         SBO's Advanced > Chat: hud_chat_fontsize, hud_colortext 1/2, hud_saytext_sound, hud_saytext_time
//   General      cl_discord_rpc, cl_experimental_menus, cl_new_ingame_menus, the version and the update check
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <FileSystem.h>
#include <IBaseUI.h>
#include <IGameUIFuncs.h>
#include <KeyValues.h>
#include <tier1/checksum_md5.h>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>
#include <IEngineVGui.h>
#include <vgui/IPanel.h>

#include "client_viewport.h"
#include "hud_chat_color_picker.h"
#include "sbo_bridge.h"
#include "vgui2_api.h"
#include "sbo_options.h"
#include "sbo_options_window.h"
#include "sbo_create_server.h"
#include "sbo_server_browser.h"
#include "sbo_script_file.h"
#include "sbo_settings_window.h"
#include "sbo_ui_style.h"
#include "../spray_anim_core.h"
#include "../updater/update_checker.h"
#include "../version/sbo_version.h"

#ifdef PostMessage
#undef PostMessage
#endif

using namespace sbolook;

namespace
{
//-----------------------------------------------------------------------------
// Engine helpers
//-----------------------------------------------------------------------------
float CvarF(const char *name)
{
	sbo::CvarHandle h = sbo::FindCvar(name);
	return h ? sbo::CvarGetFloat(h) : 0.0f;
}

std::string CvarS(const char *name)
{
	sbo::CvarHandle h = sbo::FindCvar(name);
	return h ? sbo::CvarGetString(h) : "";
}

bool ReadFile(const char *path, std::string &out, unsigned maxBytes, const char *pathID = nullptr)
{
	out.clear();
	FileHandle_t fh = g_pFullFileSystem->Open(path, "rb", pathID);
	if (!fh)
		return false;
	unsigned size = g_pFullFileSystem->Size(fh);
	if (size > maxBytes)
		size = maxBytes;
	out.resize(size);
	if (size > 0 && g_pFullFileSystem->Read(&out[0], (int)size, fh) != (int)size)
		out.clear();
	g_pFullFileSystem->Close(fh);
	return !out.empty();
}

bool WriteFile(const char *path, const std::string &data, const char *pathID)
{
	FileHandle_t fh = g_pFullFileSystem->Open(path, "wb", pathID);
	if (!fh)
		return false;
	g_pFullFileSystem->Write(data.data(), (int)data.size(), fh);
	g_pFullFileSystem->Close(fh);
	return true;
}

//! A "#token" text through the game's localization (as VGUI2 labels do), any other text as it is
std::string LocalizedText(const std::string &text)
{
	if (text.size() > 1 && text[0] == '#' && g_pVGuiLocalize)
	{
		if (const wchar_t *w = g_pVGuiLocalize->Find(text.c_str()))
		{
			char buf[512];
			g_pVGuiLocalize->ConvertUnicodeToANSI(w, buf, sizeof(buf));
			return buf;
		}
	}
	return text;
}

//! "command" "text" pairs of a GameUI list file (kb_act.lst, kb_def.lst): quoted or bare tokens, // comments
std::vector<std::pair<std::string, std::string>> ParsePairs(const std::string &text, size_t maxPairs)
{
	std::vector<std::string> toks;
	size_t i = 0;
	while (i < text.size() && toks.size() < maxPairs * 2)
	{
		const char c = text[i];
		if ((unsigned char)c <= ' ')
		{
			i++;
			continue;
		}
		if (c == '/' && i + 1 < text.size() && text[i + 1] == '/')
		{
			while (i < text.size() && text[i] != '\n')
				i++;
			continue;
		}
		std::string tok;
		if (c == '"')
		{
			i++;
			while (i < text.size() && text[i] != '"' && tok.size() < 128)
				tok += text[i++];
			while (i < text.size() && text[i] != '"')
				i++;
			i++;
		}
		else
		{
			while (i < text.size() && (unsigned char)text[i] > ' ' && tok.size() < 128)
				tok += text[i++];
			while (i < text.size() && (unsigned char)text[i] > ' ')
				i++;
		}
		toks.push_back(tok);
	}
	std::vector<std::pair<std::string, std::string>> pairs;
	for (size_t k = 0; k + 1 < toks.size(); k += 2)
		pairs.push_back(std::make_pair(toks[k], toks[k + 1]));
	return pairs;
}

//-----------------------------------------------------------------------------
// Spray (GameUI's COptionsSubMultiplayer logo list, RemapLogo and UpdateLogoWAD)
//-----------------------------------------------------------------------------
struct LogoColor
{
	const char *name; //!< cl_logocolor value
	const char *label;
	int r, g, b;
};
// GameUI's nine colors (gameui.so 0x14f720)
const LogoColor s_LogoColors[] = {
	{ "#Valve_Orange", "Orange", 255, 120, 24 }, { "#Valve_Yellow", "Yellow", 225, 180, 24 },
	{ "#Valve_Blue", "Blue", 0, 60, 255 }, { "#Valve_Ltblue", "Light blue", 0, 167, 255 },
	{ "#Valve_Green", "Green", 0, 167, 0 }, { "#Valve_Red", "Red", 255, 43, 0 },
	{ "#Valve_Brown", "Brown", 123, 73, 0 }, { "#Valve_Ltgray", "Light gray", 100, 100, 100 },
	{ "#Valve_Dkgray", "Dark gray", 36, 36, 36 },
};
constexpr int LOGO_COLOR_COUNT = (int)(sizeof(s_LogoColors) / sizeof(s_LogoColors[0]));

//! logos/*.bmp, without the extension, sorted, remapped.bmp left out (GameUI's InitLogoList)
std::vector<std::string> ListLogos()
{
	std::vector<std::string> names;
	FileFindHandle_t fh;
	const char *fn = g_pFullFileSystem->FindFirst("logos/*.*", &fh);
	while (fn && names.size() < 512)
	{
		std::string s = fn;
		if (s.size() > 4 && s[0] != '.' && !Q_stricmp(s.c_str() + s.size() - 4, ".bmp") && Q_stricmp(s.c_str(), "remapped.bmp"))
		{
			s.erase(s.size() - 4);
			bool dup = false;
			for (const std::string &n : names)
				dup = dup || !Q_stricmp(n.c_str(), s.c_str());
			if (!dup)
				names.push_back(s);
		}
		fn = g_pFullFileSystem->FindNext(fh);
	}
	g_pFullFileSystem->FindClose(fh);
	std::sort(names.begin(), names.end(), [](const std::string &a, const std::string &b) { return Q_stricmp(a.c_str(), b.c_str()) < 0; });
	return names;
}

void PutInt(std::string &out, int v)
{
	for (int i = 0; i < 4; i++)
		out += (char)((v >> (8 * i)) & 255);
}

//! tempdecal.wad from an 8-bit logo BMP in the color (GameUI's RemapLogo palette + UpdateLogoWAD/GrabMip): one
//! miptex lump "LOGO" (type 0x40), the four mips by point sampling, the palette from black to the color with the
//! color itself as index 255. Only 16, 32 or 64 square logos, as GameUI. False if the file is not usable.
bool BuildLogoWad(const std::string &logo, int r, int g, int b, std::string &wad)
{
	std::string bmp;
	if (!ReadFile(("logos/" + logo + ".bmp").c_str(), bmp, 256 * 1024))
		return false;
	if (bmp.size() < 54 || bmp[0] != 'B' || bmp[1] != 'M')
		return false;
	auto U32 = [&](size_t o) { return (unsigned)(unsigned char)bmp[o] | ((unsigned)(unsigned char)bmp[o + 1] << 8) |
		                              ((unsigned)(unsigned char)bmp[o + 2] << 16) | ((unsigned)(unsigned char)bmp[o + 3] << 24); };
	auto U16 = [&](size_t o) { return (unsigned)(unsigned char)bmp[o] | ((unsigned)(unsigned char)bmp[o + 1] << 8); };
	const unsigned headerSize = U32(14);
	const int w = (int)U32(18), h = (int)U32(22);
	const unsigned bits = U16(28);
	if (headerSize != 40 || bits != 8 || w != h || (w != 16 && w != 32 && w != 64))
		return false;
	const size_t pixels = 14 + 40 + 256 * 4; // GameUI reads the image right after a 256-color palette
	if (bmp.size() < pixels + (size_t)w * h)
		return false;
	std::string lump;
	char name[16] = "LOGO";
	lump.append(name, 16);
	PutInt(lump, w);
	PutInt(lump, h);
	const int headerLen = 16 + 8 + 16;
	int offsets[4];
	offsets[0] = headerLen;
	offsets[1] = offsets[0] + w * h;
	offsets[2] = offsets[1] + (w / 2) * (h / 2);
	offsets[3] = offsets[2] + (w / 4) * (h / 4);
	for (int o : offsets)
		PutInt(lump, o);
	// mip 0: the rows top down (the BMP's are bottom up)
	std::vector<unsigned char> img((size_t)w * h);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			img[(size_t)y * w + x] = (unsigned char)bmp[pixels + (size_t)(h - 1 - y) * w + x];
	lump.append((const char *)img.data(), img.size());
	for (int mip = 1; mip < 4; mip++)
	{
		const int step = 1 << mip;
		for (int y = 0; y < h; y += step)
			for (int x = 0; x < w; x += step)
				lump += (char)img[(size_t)y * w + x]; // GameUI's AveragePixels: the block's first pixel
	}
	lump += (char)0;
	lump += (char)1; // 256 colors (short)
	for (int i = 0; i < 255; i++)
	{
		const float t = (float)i / 256.0f;
		lump += (char)(unsigned char)(r * t);
		lump += (char)(unsigned char)(g * t);
		lump += (char)(unsigned char)(b * t);
	}
	lump += (char)r;
	lump += (char)g;
	lump += (char)b;
	while (lump.size() & 3)
		lump += (char)0;
	wad = "WAD3";
	PutInt(wad, 1);
	PutInt(wad, 12 + (int)lump.size());
	wad += lump;
	PutInt(wad, 12);                 // filepos
	PutInt(wad, (int)lump.size());   // disksize
	PutInt(wad, (int)lump.size());   // size
	wad += (char)0x40;               // TYP_LUMPY
	wad += (char)0;                  // compression
	wad += (char)0;
	wad += (char)0;
	wad.append(name, 16);
	return true;
}

bool WriteLogoWad(const std::string &logo, int r, int g, int b)
{
	std::string wad;
	return BuildLogoWad(logo, r, g, b, wad) && WriteFile("tempdecal.wad", wad, "GAMECONFIG");
}

//! The first miptex of a WAD3 spray: width, height, pixels (indices) and the palette; false if it is not one
bool ReadSprayImage(const std::string &wad, int &w, int &h, const unsigned char *&pixels, const unsigned char *&palette,
    bool *transparentIndex = nullptr)
{
	auto U32 = [&](size_t o) { return o + 4 <= wad.size() ? (unsigned)(unsigned char)wad[o] | ((unsigned)(unsigned char)wad[o + 1] << 8) |
		                                                       ((unsigned)(unsigned char)wad[o + 2] << 16) | ((unsigned)(unsigned char)wad[o + 3] << 24) : 0u; };
	if (wad.size() < 12 || wad.compare(0, 4, "WAD3") != 0 || U32(4) < 1)
		return false;
	const size_t info = U32(8);
	if (info + 32 > wad.size())
		return false;
	const size_t pos = U32(info);
	if (pos + 40 > wad.size())
		return false;
	w = (int)U32(pos + 16);
	h = (int)U32(pos + 20);
	if (w <= 0 || h <= 0 || w > 256 || h > 256)
		return false;
	const size_t off0 = U32(pos + 24), off3 = U32(pos + 36);
	const size_t pal = pos + off3 + (size_t)(w / 8) * (h / 8) + 2;
	if (pos + off0 + (size_t)w * h > wad.size() || pal + 768 > wad.size())
		return false;
	pixels = (const unsigned char *)wad.data() + pos + off0;
	palette = (const unsigned char *)wad.data() + pal;
	if (transparentIndex)
		*transparentIndex = wad[pos] == '{'; // "{name": a color image with index 255 transparent; else a decal
	return true;
}

//-----------------------------------------------------------------------------
// Engine key names for VGUI2 key codes and mouse buttons (the names bind / Key_KeyStringToKeyNum take)
//-----------------------------------------------------------------------------
const char *EngineKeyName(vgui2::KeyCode code)
{
	static char s_Buf[4];
	if (code >= vgui2::KEY_A && code <= vgui2::KEY_Z)
	{
		s_Buf[0] = (char)('a' + (code - vgui2::KEY_A));
		s_Buf[1] = 0;
		return s_Buf;
	}
	if (code >= vgui2::KEY_0 && code <= vgui2::KEY_9)
	{
		s_Buf[0] = (char)('0' + (code - vgui2::KEY_0));
		s_Buf[1] = 0;
		return s_Buf;
	}
	if (code >= vgui2::KEY_F1 && code <= vgui2::KEY_F12)
	{
		static char s_F[4];
		snprintf(s_F, sizeof(s_F), "F%d", 1 + (code - vgui2::KEY_F1));
		return s_F;
	}
	switch (code)
	{
	case vgui2::KEY_PAD_0: return "KP_INS";
	case vgui2::KEY_PAD_1: return "KP_END";
	case vgui2::KEY_PAD_2: return "KP_DOWNARROW";
	case vgui2::KEY_PAD_3: return "KP_PGDN";
	case vgui2::KEY_PAD_4: return "KP_LEFTARROW";
	case vgui2::KEY_PAD_5: return "KP_5";
	case vgui2::KEY_PAD_6: return "KP_RIGHTARROW";
	case vgui2::KEY_PAD_7: return "KP_HOME";
	case vgui2::KEY_PAD_8: return "KP_UPARROW";
	case vgui2::KEY_PAD_9: return "KP_PGUP";
	case vgui2::KEY_PAD_DIVIDE: return "KP_SLASH";
	case vgui2::KEY_PAD_MULTIPLY: return "*";
	case vgui2::KEY_PAD_MINUS: return "KP_MINUS";
	case vgui2::KEY_PAD_PLUS: return "KP_PLUS";
	case vgui2::KEY_PAD_ENTER: return "KP_ENTER";
	case vgui2::KEY_PAD_DECIMAL: return "KP_DEL";
	case vgui2::KEY_LBRACKET: return "[";
	case vgui2::KEY_RBRACKET: return "]";
	case vgui2::KEY_SEMICOLON: return "SEMICOLON";
	case vgui2::KEY_APOSTROPHE: return "'";
	case vgui2::KEY_BACKQUOTE: return "`";
	case vgui2::KEY_COMMA: return ",";
	case vgui2::KEY_PERIOD: return ".";
	case vgui2::KEY_SLASH: return "/";
	case vgui2::KEY_BACKSLASH: return "\\";
	case vgui2::KEY_MINUS: return "-";
	case vgui2::KEY_EQUAL: return "=";
	case vgui2::KEY_ENTER: return "ENTER";
	case vgui2::KEY_SPACE: return "SPACE";
	case vgui2::KEY_BACKSPACE: return "BACKSPACE";
	case vgui2::KEY_TAB: return "TAB";
	case vgui2::KEY_CAPSLOCK: return "CAPSLOCK";
	case vgui2::KEY_INSERT: return "INS";
	case vgui2::KEY_DELETE: return "DEL";
	case vgui2::KEY_HOME: return "HOME";
	case vgui2::KEY_END: return "END";
	case vgui2::KEY_PAGEUP: return "PGUP";
	case vgui2::KEY_PAGEDOWN: return "PGDN";
	case vgui2::KEY_BREAK: return "PAUSE";
	case vgui2::KEY_LSHIFT:
	case vgui2::KEY_RSHIFT: return "SHIFT";
	case vgui2::KEY_LALT:
	case vgui2::KEY_RALT: return "ALT";
	case vgui2::KEY_LCONTROL:
	case vgui2::KEY_RCONTROL: return "CTRL";
	case vgui2::KEY_UP: return "UPARROW";
	case vgui2::KEY_LEFT: return "LEFTARROW";
	case vgui2::KEY_DOWN: return "DOWNARROW";
	case vgui2::KEY_RIGHT: return "RIGHTARROW";
	default: return nullptr;
	}
}

const char *EngineMouseName(vgui2::MouseCode code)
{
	switch (code)
	{
	case vgui2::MOUSE_LEFT: return "MOUSE1";
	case vgui2::MOUSE_RIGHT: return "MOUSE2";
	case vgui2::MOUSE_MIDDLE: return "MOUSE3";
	case vgui2::MOUSE_4: return "MOUSE4";
	case vgui2::MOUSE_5: return "MOUSE5";
	default: return nullptr;
	}
}

//! The engine knows the key name (bind would take it)
bool KnownKey(const char *name)
{
	if (!name || !name[0])
		return false;
	return !g_pGameUIFuncs || g_pGameUIFuncs->Key_KeyStringToKeyNum(name) >= 0;
}

//! Upper-case display text of a key name ("w" -> "W", "UPARROW" -> "UP ARROW")
std::string KeyDisplay(const std::string &key)
{
	static const struct
	{
		const char *key, *text;
	} s_Names[] = { { "UPARROW", "UP ARROW" }, { "DOWNARROW", "DOWN ARROW" }, { "LEFTARROW", "LEFT ARROW" },
		{ "RIGHTARROW", "RIGHT ARROW" }, { "MWHEELUP", "MOUSE WHEEL UP" }, { "MWHEELDOWN", "MOUSE WHEEL DOWN" },
		{ "SEMICOLON", ";" } };
	for (const auto &n : s_Names)
	{
		if (!Q_stricmp(key.c_str(), n.key))
			return n.text;
	}
	std::string s = key;
	for (char &c : s)
		c = (char)toupper((unsigned char)c);
	return s;
}
} // namespace

//-----------------------------------------------------------------------------
// Spray preview (GameUI's Multiplayer page shows the logo): the player's tempdecal.wad as it is (animated sprays play,
// spray_anim_core.h), or the logo and color chosen in the lists before they are applied
//-----------------------------------------------------------------------------
class CSboSprayPreview : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboSprayPreview, vgui2::Panel);

public:
	CSboSprayPreview(vgui2::Panel *pParent)
	    : BaseClass(pParent, "SprayPreview")
	{
		SetMouseInputEnabled(false);
	}
	~CSboSprayPreview()
	{
		if (m_iTexture != -1)
			vgui2::surface()->DeleteTextureByID(m_iTexture);
	}

	//! The player's current spray (tempdecal.wad in the mod folder) when it is an animated one; false otherwise
	bool ShowCurrentIfAnimated()
	{
		std::string wad;
		ReadFile("tempdecal.wad", wad, 64 * 1024, "GAMECONFIG");
		SprayAnimInfo info;
		if (!SprayAnim_Parse((const unsigned char *)wad.data(), (int)wad.size(), info))
			return false;
		SetWad(wad);
		return true;
	}
	//! A logo in a color, as Apply would write it
	void ShowLogo(const std::string &logo, int r, int g, int b)
	{
		std::string wad;
		BuildLogoWad(logo, r, g, b, wad);
		SetWad(wad);
	}

	void OnThink() override
	{
		BaseClass::OnThink();
		if (!m_bAnimated)
			return;
		const unsigned now = (unsigned)(vgui2::system()->GetTimeMillis() - m_uStart);
		const int frame = SprayAnim_FrameAt(m_Anim, now);
		if (frame == m_iFrame)
			return;
		if (frame < m_iFrame)
		{
			memcpy(m_Pixels.data(), m_Anim.frame0, m_Pixels.size());
			m_iFrame = 0;
		}
		while (m_iFrame < frame)
		{
			if (!SprayAnim_Decode(m_Anim, m_iFrame + 1, m_Pixels.data()))
			{
				m_bAnimated = false;
				break;
			}
			m_iFrame++;
		}
		Upload();
	}

	void PaintBackground() override
	{
		// a dark checkerboard under the transparent parts
		const int s = std::max(4, S().M(8));
		for (int y = 0; y < GetTall(); y += s)
			for (int x = 0; x < GetWide(); x += s)
				FillRect(x, y, std::min(s, GetWide() - x), std::min(s, GetTall() - y), ((x / s + y / s) & 1) ? Color(24, 28, 24, 255) : Color(16, 19, 16, 255));
	}
	void PaintBorder() override { OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER_LIST); }
	void Paint() override
	{
		if (m_iTexture == -1 || m_iW <= 0 || m_iH <= 0)
		{
			DrawTextMid(S().small, COL_TEXT_FAINT, S().M(6), 0, GetTall(), L"No spray", GetWide() - S().M(12));
			return;
		}
		const float f = std::min((float)(GetWide() - 4) / m_iW, (float)(GetTall() - 4) / m_iH);
		const int dw = (int)(m_iW * f), dh = (int)(m_iH * f);
		const int dx = (GetWide() - dw) / 2, dy = (GetTall() - dh) / 2;
		vgui2::surface()->DrawSetTexture(m_iTexture);
		vgui2::surface()->DrawSetColor(255, 255, 255, 255);
		vgui2::surface()->DrawTexturedRect(dx, dy, dx + dw, dy + dh);
	}

private:
	void SetWad(const std::string &wad)
	{
		m_Wad = wad;
		m_bAnimated = false;
		m_iW = m_iH = 0;
		const unsigned char *pixels = nullptr, *palette = nullptr;
		m_bTransparentIndex = true;
		if (SprayAnim_Parse((const unsigned char *)m_Wad.data(), (int)m_Wad.size(), m_Anim))
		{
			m_bAnimated = true;
			m_iW = m_Anim.width;
			m_iH = m_Anim.height;
			m_pPalette = m_Anim.palette;
			m_Pixels.assign(m_Anim.frame0, m_Anim.frame0 + (size_t)m_iW * m_iH);
			m_iFrame = 0;
			m_uStart = (unsigned)vgui2::system()->GetTimeMillis();
		}
		else if (ReadSprayImage(m_Wad, m_iW, m_iH, pixels, palette, &m_bTransparentIndex))
		{
			m_pPalette = palette;
			m_Pixels.assign(pixels, pixels + (size_t)m_iW * m_iH);
		}
		Upload();
	}

	void Upload()
	{
		if (m_iW <= 0 || m_iH <= 0 || m_Pixels.size() < (size_t)m_iW * m_iH)
		{
			Repaint();
			return;
		}
		std::vector<unsigned char> rgba((size_t)m_iW * m_iH * 4);
		for (size_t i = 0; i < (size_t)m_iW * m_iH; i++)
		{
			const unsigned char c = m_Pixels[i];
			if (m_bTransparentIndex)
			{
				// a "{" image (SBO's animated sprays): its palette's colors, index 255 transparent
				rgba[i * 4 + 0] = m_pPalette[c * 3 + 0];
				rgba[i * 4 + 1] = m_pPalette[c * 3 + 1];
				rgba[i * 4 + 2] = m_pPalette[c * 3 + 2];
				rgba[i * 4 + 3] = c == 255 ? 0 : 255;
			}
			else
			{
				// a decal (GameUI's "LOGO" sprays): the engine draws it in the palette's last color with the index as
				// its opacity (black = clear, the logo's bright parts solid)
				rgba[i * 4 + 0] = m_pPalette[255 * 3 + 0];
				rgba[i * 4 + 1] = m_pPalette[255 * 3 + 1];
				rgba[i * 4 + 2] = m_pPalette[255 * 3 + 2];
				rgba[i * 4 + 3] = c;
			}
		}
		if (m_iTexture == -1)
			m_iTexture = vgui2::surface()->CreateNewTextureID(true);
		vgui2::surface()->DrawSetTextureRGBA(m_iTexture, rgba.data(), m_iW, m_iH, true, true); // filtered, as GameUI's picture
		Repaint();
	}

	std::string m_Wad; //!< m_Anim and m_pPalette point into it
	SprayAnimInfo m_Anim = {};
	bool m_bAnimated = false;
	bool m_bTransparentIndex = true;
	const unsigned char *m_pPalette = nullptr;
	std::vector<unsigned char> m_Pixels;
	int m_iW = 0, m_iH = 0, m_iFrame = 0;
	unsigned m_uStart = 0;
	int m_iTexture = -1;
};

//-----------------------------------------------------------------------------
// Keyboard: the action list (GameUI's COptionsSubKeyboard)
//-----------------------------------------------------------------------------
class CSboKeyList : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboKeyList, vgui2::Panel);

public:
	struct Item
	{
		std::string command;     //!< "blank" for a heading row
		std::string description;
		std::string key, alt;    //!< engine key names
	};

	CSboKeyList(vgui2::Panel *pParent)
	    : BaseClass(pParent, "KeyList")
	{
		SetKeyBoardInputEnabled(true);
		SetMouseInputEnabled(true);
	}

	std::vector<Item> m_Items;
	std::vector<std::string> m_Removed; //!< keys to unbind on Apply
	int m_iSelected = -1, m_iFirst = 0;
	bool m_bCapturing = false;
	bool m_bChanged = false;

	void Load(bool defaults)
	{
		std::string text;
		if (!defaults)
		{
			m_Items.clear();
			ReadFile("gfx/shell/kb_act.lst", text, 64 * 1024);
			for (const auto &p : ParsePairs(text, 512))
			{
				Item it;
				it.command = p.first;
				it.description = p.second;
				if (it.description.size() > 1 && it.description[0] == '#' && g_pVGuiLocalize)
				{
					if (const wchar_t *w = g_pVGuiLocalize->Find(it.description.c_str()))
					{
						char buf[256];
						g_pVGuiLocalize->ConvertUnicodeToANSI(w, buf, sizeof(buf));
						it.description = buf;
					}
				}
				if (it.command == "blank")
				{
					// SBO's kb_act.lst frames its headings in VGUI1-style star lines ("******Action Commands******"
					// between two "****...****" rows): the heading keeps its words, a row of only stars is dropped
					std::string h;
					for (char ch : it.description)
					{
						if (ch == '*' || ch == ' ' || ch == '\t' || ch == '=')
						{
							if (!h.empty() && h.back() != ' ')
								h += ' ';
							continue;
						}
						h += ch;
					}
					while (!h.empty() && h.back() == ' ')
						h.pop_back();
					if (h.empty())
						continue;
					it.description = h;
				}
				m_Items.push_back(it);
			}
			FillCurrent();
			m_Removed.clear();
			m_bChanged = false;
		}
		else
		{
			// GameUI's FillInDefaultBindings: every binding cleared (each key taken off is unbound on Apply), then
			// kb_def.lst; toggleconsole and cancelselect keep ` and ESCAPE
			for (Item &it : m_Items)
			{
				for (std::string *k : { &it.key, &it.alt })
				{
					if (!k->empty())
						m_Removed.push_back(*k);
					k->clear();
				}
			}
			ReadFile("gfx/shell/kb_def.lst", text, 64 * 1024);
			for (const auto &p : ParsePairs(text, 512))
				Bind(p.second, p.first);
			if (Item *it = Find("toggleconsole"))
			{
				if (it->key.empty())
					Bind("toggleconsole", "`");
			}
			if (Item *it = Find("cancelselect"))
			{
				if (it->key.empty())
					Bind("cancelselect", "ESCAPE");
			}
			m_bChanged = true;
		}
		Repaint();
	}

	Item *Find(const std::string &command)
	{
		for (Item &it : m_Items)
		{
			if (!Q_stricmp(it.command.c_str(), command.c_str()))
				return &it;
		}
		return nullptr;
	}

	//! GameUI's AddBinding: the new key becomes the key, the old key the alternate
	void Bind(const std::string &command, const std::string &key)
	{
		if (Item *it = Find(command))
		{
			if (!Q_stricmp(it->key.c_str(), key.c_str()) || !Q_stricmp(it->alt.c_str(), key.c_str()))
				return;
			if (!it->alt.empty())
				m_Removed.push_back(it->alt);
			it->alt = it->key;
			it->key = key;
		}
	}

	void FillCurrent()
	{
		if (!g_pGameUIFuncs)
			return;
		for (int k = 0; k < 256; k++)
		{
			const char *binding = g_pGameUIFuncs->Key_BindingForKey(k);
			const char *name = g_pGameUIFuncs->Key_NameForKey(k);
			if (!binding || !binding[0] || !name || !name[0])
				continue;
			Bind(binding, name);
		}
	}

	//! The key taken from every other action (one key, one action), then given to the selected one
	void SetKey(const std::string &key)
	{
		if (m_iSelected < 0 || m_iSelected >= (int)m_Items.size())
			return;
		for (Item &it : m_Items)
		{
			if (!Q_stricmp(it.key.c_str(), key.c_str()))
			{
				it.key = it.alt;
				it.alt.clear();
			}
			else if (!Q_stricmp(it.alt.c_str(), key.c_str()))
				it.alt.clear();
		}
		Item &sel = m_Items[m_iSelected];
		if (!sel.alt.empty())
			m_Removed.push_back(sel.alt);
		sel.alt = sel.key;
		sel.key = key;
		m_bChanged = true;
		Notify();
	}

	void ClearSelected()
	{
		if (m_iSelected < 0 || m_iSelected >= (int)m_Items.size())
			return;
		Item &sel = m_Items[m_iSelected];
		for (std::string *k : { &sel.key, &sel.alt })
		{
			if (!k->empty())
				m_Removed.push_back(*k);
			k->clear();
		}
		m_bChanged = true;
		Notify();
	}

	void Apply()
	{
		// GameUI's ApplyAllBindings
		for (const std::string &k : m_Removed)
		{
			bool still = false;
			for (const Item &it : m_Items)
				still = still || !Q_stricmp(it.key.c_str(), k.c_str()) || !Q_stricmp(it.alt.c_str(), k.c_str());
			if (still)
				continue;
			char cmd[128];
			snprintf(cmd, sizeof(cmd), "unbind \"%s\"\n", k.c_str());
			sbo::ExecClientCmd(cmd);
		}
		m_Removed.clear();
		for (const Item &it : m_Items)
		{
			if (it.command == "blank")
				continue;
			for (const std::string *k : { &it.key, &it.alt })
			{
				if (k->empty())
					continue;
				char cmd[384];
				snprintf(cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n", k->c_str(), it.command.c_str());
				sbo::ExecClientCmd(cmd);
			}
		}
		sbo::ExecClientCmd("exec userconfig.cfg\n");
		m_bChanged = false;
	}

	void Notify()
	{
		Repaint();
		if (GetParent())
			vgui2::ivgui()->PostMessage(GetParent()->GetVPanel(), new KeyValues("SettingsChanged"), GetVPanel());
	}

	int RowTall() { return S().M(23); }
	int HeadTall() { return S().M(26); }
	int RowsFit() { return std::max(1, (GetTall() - HeadTall()) / RowTall()); }

	void ScrollTo(int row)
	{
		if (row < m_iFirst)
			m_iFirst = row;
		if (row >= m_iFirst + RowsFit())
			m_iFirst = row - RowsFit() + 1;
		m_iFirst = std::max(0, std::min(m_iFirst, (int)m_Items.size() - RowsFit()));
	}

	void Capture(const char *key)
	{
		m_bCapturing = false;
		vgui2::input()->SetMouseCapture(0);
		if (key && KnownKey(key))
			SetKey(key);
		Repaint();
	}

	void OnMousePressed(vgui2::MouseCode code) override
	{
		if (m_bCapturing)
		{
			Capture(EngineMouseName(code));
			return;
		}
		RequestFocus();
		int x, y;
		vgui2::input()->GetCursorPos(x, y);
		ScreenToLocal(x, y);
		const int row = m_iFirst + (y - HeadTall()) / RowTall();
		if (y >= HeadTall() && row >= 0 && row < (int)m_Items.size() && m_Items[row].command != "blank")
		{
			m_iSelected = row;
			Repaint();
		}
	}

	void OnMouseDoublePressed(vgui2::MouseCode code) override
	{
		if (code == vgui2::MOUSE_LEFT && m_iSelected >= 0 && !m_bCapturing)
			StartCapture();
	}

	void StartCapture()
	{
		if (m_iSelected < 0)
			return;
		m_bCapturing = true;
		RequestFocus();
		vgui2::input()->SetMouseCapture(GetVPanel());
		Repaint();
	}

	void OnMouseWheeled(int delta) override
	{
		if (m_bCapturing)
		{
			Capture(delta > 0 ? "MWHEELUP" : "MWHEELDOWN");
			return;
		}
		m_iFirst = std::max(0, std::min(m_iFirst - delta * 3, (int)m_Items.size() - RowsFit()));
		Repaint();
	}

	void OnKeyCodePressed(vgui2::KeyCode code) override
	{
		if (m_bCapturing)
		{
			// Escape cancels the capture (GameUI's); every other key is bound
			m_bJustCaptured = true; // the typed event of this key follows
			if (code == vgui2::KEY_ESCAPE)
			{
				m_bCapturing = false;
				vgui2::input()->SetMouseCapture(0);
				Repaint();
				return;
			}
			Capture(EngineKeyName(code));
			return;
		}
		if (code == vgui2::KEY_ENTER)
		{
			StartCapture();
			return;
		}
		if (code == vgui2::KEY_DELETE)
		{
			ClearSelected();
			return;
		}
		if (code == vgui2::KEY_UP || code == vgui2::KEY_DOWN)
		{
			int r = m_iSelected;
			do
				r += code == vgui2::KEY_UP ? -1 : 1;
			while (r >= 0 && r < (int)m_Items.size() && m_Items[r].command == "blank");
			if (r >= 0 && r < (int)m_Items.size())
			{
				m_iSelected = r;
				ScrollTo(r);
				Repaint();
			}
			return;
		}
		BaseClass::OnKeyCodePressed(code);
	}

	void OnKeyCodeTyped(vgui2::KeyCode code) override
	{
		// a key being captured (and the Escape that cancels it) must not reach the window (Escape closes it)
		if (m_bCapturing || m_bJustCaptured)
		{
			m_bJustCaptured = false;
			return;
		}
		BaseClass::OnKeyCodeTyped(code);
	}
	bool m_bJustCaptured = false;

	void PaintBackground() override { FillRect(0, 0, GetWide(), GetTall(), COL_BG_INPUT); }
	void PaintBorder() override { OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER_LIST); }

	void Paint() override
	{
		const Style &st = S();
		const int W = GetWide(), pad = st.M(8);
		const int keyX = W * 58 / 100, altX = W * 78 / 100;
		const int ht = HeadTall(), rt = RowTall();
		DrawSpacedUpper(st.small, COL_TEXT_DIM, pad, (ht - FontTall(st.small)) / 2, "Action");
		DrawSpacedUpper(st.small, COL_TEXT_DIM, keyX, (ht - FontTall(st.small)) / 2, "Key");
		DrawSpacedUpper(st.small, COL_TEXT_DIM, altX, (ht - FontTall(st.small)) / 2, "Alternate");
		FillRect(0, ht - 1, W, 1, COL_BORDER_LIST);
		const int rows = RowsFit();
		for (int i = 0; i < rows && m_iFirst + i < (int)m_Items.size(); i++)
		{
			const Item &it = m_Items[m_iFirst + i];
			const int y = ht + i * rt;
			if (it.command == "blank")
			{
				// section heading: the cards' heading style, then a line to the right edge
				const int ty = y + (rt - FontTall(st.small)) / 2;
				const int tw = DrawSpacedUpper(st.small, COL_TEXT_BRIGHT, pad, ty, it.description.c_str());
				FillRect(pad + tw + st.M(10), y + rt / 2, std::max(0, W - pad - (pad + tw + st.M(10))), 1, COL_BORDER_LIST);
				continue;
			}
			const bool sel = m_iFirst + i == m_iSelected;
			if (sel)
				FillRect(1, y, W - 2, rt, COL_BG_SELECTED);
			const Color c = sel ? COL_TEXT_SELECTED : COL_TEXT_LIGHT;
			DrawTextMid(st.row, c, pad, y, rt, Widen(it.description.c_str()), keyX - 2 * pad);
			if (sel && m_bCapturing)
				DrawTextMid(st.row, COL_TEXT_BRIGHT, keyX, y, rt, L"Press a key (Esc: cancel)");
			else
			{
				DrawTextMid(st.row, c, keyX, y, rt, Widen(KeyDisplay(it.key).c_str()), altX - keyX - pad);
				DrawTextMid(st.row, COL_TEXT_DIM, altX, y, rt, Widen(KeyDisplay(it.alt).c_str()), W - altX - pad);
			}
			FillRect(1, y + rt - 1, W - 2, 1, Color(44, 90, 37, 90));
		}
	}
};

//-----------------------------------------------------------------------------
// The window
//-----------------------------------------------------------------------------
namespace
{
enum OptionsTab
{
	TAB_MULTIPLAYER = 0,
	TAB_KEYBOARD,
	TAB_MOUSE,
	TAB_AUDIO,
	TAB_VIDEO,
	TAB_VOICE,
	TAB_CONTENT,
	TAB_CHAT,
	TAB_GENERAL,
	TAB_VERSION,
	TAB_COUNT
};
const char *const s_TabNames[TAB_COUNT] = { "Multiplayer", "Keyboard", "Mouse", "Audio", "Video", "Voice", "Content",
	"Chat", "General", "Version" };
// the Options window: GameUI's Options pages; the Advanced window: SBO's Advanced dialog's tabs
const int s_OptionsTabs[] = { TAB_MULTIPLAYER, TAB_KEYBOARD, TAB_MOUSE, TAB_AUDIO, TAB_VIDEO, TAB_VOICE, TAB_CONTENT };
const int s_AdvancedTabs[] = { TAB_GENERAL, TAB_CHAT, TAB_VERSION };

// The engine's voice tweak interface as HL25 has it (cl_enginefunc_t::pVoiceTweak). Its fifth member is not the
// Source SDK's IsStillTweaking: HL25 GameUI's COptionsSubVoice::OnThink (gameui.so 0xb15e8) calls it for the
// microphone level and scales the result as 0..32767 (value * 160 >> 15 = the meter's width in its units)
struct VoiceTweak
{
	int (*StartVoiceTweakMode)();
	void (*EndVoiceTweakMode)();
	void (*SetControlFloat)(int iControl, float value);
	float (*GetControlFloat)(int iControl);
	int (*GetSpeakingVolume)(); // 0..32767
};
constexpr int VOICE_MIC_VOLUME = 0, VOICE_MIC_BOOST = 2;

VoiceTweak *Voice()
{
	return static_cast<VoiceTweak *>(sbo::GetVoiceTweak());
}

//! IGameUIFuncs::GetCurrentRenderer. HL25 dropped its last argument (vid level); on Windows the callee pops its own
//! arguments (thiscall), so the call keeps esp itself and works with either count.
void CurrentRenderer(char *name, int nameLen, int *windowed, int *hdmodels, int *addons)
{
	int vidLevel = 0;
	*windowed = *hdmodels = *addons = 0;
	name[0] = 0;
	if (!g_pGameUIFuncs)
		return;
#if defined(_WIN32) && defined(_M_IX86)
	IGameUIFuncs *p = g_pGameUIFuncs;
	void *fn = (*reinterpret_cast<void ***>(p))[7]; // ~IBaseInterface, IsKeyDown, Key_NameForKey, Key_BindingForKey,
	                                                // GetVGUI2KeyCodeForBind, GetVideoModes, GetCurrentVideoMode
	int *pVid = &vidLevel;
	__asm {
		mov esi, esp
		push pVid
		push addons
		push hdmodels
		push windowed
		push nameLen
		push name
		mov ecx, p
		call fn
		mov esp, esi
	}
#else
	g_pGameUIFuncs->GetCurrentRenderer(name, nameLen, windowed, hdmodels, addons, &vidLevel);
#endif
	name[nameLen - 1] = 0;
}

//! A page with a microphone level meter (the Voice tab's test)
class CSboVoicePage : public CSboSettingsPage
{
	DECLARE_CLASS_SIMPLE(CSboVoicePage, CSboSettingsPage);

public:
	CSboVoicePage(vgui2::Panel *pParent)
	    : BaseClass(pParent, "VoicePage")
	{
	}
	~CSboVoicePage() { StopTest(); }

	CSboCustomSetting *m_pMicVolume = nullptr, *m_pBoost = nullptr;
	CSboCvarSlider *m_pMaxGain = nullptr, *m_pAvgGain = nullptr; //!< voice_maxgain, voice_avggain
	CSboBrowserButton *m_pTest = nullptr;
	bool m_bTesting = false;
	float m_flSavedVolume = 0.0f, m_flSavedBoost = 0.0f, m_flSavedScale = 0.0f;
	float m_flLevel = 0.0f;

	void StartTest()
	{
		VoiceTweak *v = Voice();
		if (!v || m_bTesting || !v->StartVoiceTweakMode())
			return;
		m_bTesting = true;
		sbo::ConsolePrint("Microphone test: voice tweak mode started\n");
		m_flSavedVolume = v->GetControlFloat(VOICE_MIC_VOLUME);
		m_flSavedBoost = v->GetControlFloat(VOICE_MIC_BOOST);
		m_flSavedScale = CvarF("voice_scale");
		PushTestValues();
		m_pTest->SetText("Stop microphone test");
		InvalidateLayout();
	}

	void PushTestValues()
	{
		VoiceTweak *v = Voice();
		if (!v || !m_bTesting)
			return;
		v->SetControlFloat(VOICE_MIC_VOLUME, SboValueSliderOf(m_pMicVolume->control)->GetValue());
		v->SetControlFloat(VOICE_MIC_BOOST, static_cast<CSboBrowserCheck *>(m_pBoost->control)->IsChecked() ? 1.0f : 0.0f);
	}

	void StopTest()
	{
		VoiceTweak *v = Voice();
		if (!m_bTesting || !v)
			return;
		v->EndVoiceTweakMode();
		sbo::ConsolePrint("Microphone test: voice tweak mode ended\n");
		// GameUI restores the values from before the test (the dialog's Apply writes the chosen ones)
		v->SetControlFloat(VOICE_MIC_VOLUME, m_flSavedVolume);
		v->SetControlFloat(VOICE_MIC_BOOST, m_flSavedBoost);
		sbo::CvarSetFloat("voice_scale", m_flSavedScale);
		m_bTesting = false;
		m_flLevel = 0.0f;
		if (m_pTest)
			m_pTest->SetText("Test microphone");
		InvalidateLayout();
		Repaint();
	}

	void OnCommand(const char *command) override
	{
		if (!Q_stricmp(command, "VoiceGainDefault"))
		{
			// HL25's defaults (hw.so: voice_maxgain "5", voice_avggain "0.5"); applied with Apply / OK
			m_pMaxGain->Slider()->SetValue(SBO_VOICE_MAXGAIN_DEFAULT);
			m_pAvgGain->Slider()->SetValue(SBO_VOICE_AVGGAIN_DEFAULT);
			m_pMaxGain->control->Repaint();
			m_pAvgGain->control->Repaint();
			NotifyChanged();
			return;
		}
		if (!Q_stricmp(command, "TestMic"))
		{
			if (m_bTesting)
				StopTest();
			else
				StartTest();
			return;
		}
		BaseClass::OnCommand(command);
	}

	void OnThink() override
	{
		BaseClass::OnThink();
		if (!m_bTesting)
			return;
		// as GameUI's OnThink: only the level meter (the test runs until Stop, a tab change or the window closing)
		VoiceTweak *v = Voice();
		if (!v)
			return;
		PushTestValues();
		m_flLevel = v->GetSpeakingVolume ? std::max(0, std::min(32767, v->GetSpeakingVolume())) / 32767.0f : 0.0f;
		Repaint();
	}

	void OnSliderMoved() override
	{
		PushTestValues();
		BaseClass::OnSliderMoved();
	}

	void Paint() override
	{
		BaseClass::Paint();
		if (!m_bTesting || !m_pTest)
			return;
		// the level meter right of the button
		int x, y, w, h;
		m_pTest->GetBounds(x, y, w, h);
		const int mx = x + w + S().M(12), mw = S().M(200);
		FillRect(mx, y + h / 3, mw, h / 3, COL_BG_INPUT);
		FillRect(mx, y + h / 3, (int)(mw * std::max(0.0f, std::min(1.0f, m_flLevel))), h / 3, COL_BORDER_PRIMARY);
		OutlineRect(mx, y + h / 3, mw, h / 3, COL_BORDER);
	}

	void SetVisible(bool state) override
	{
		if (!state)
			StopTest(); // GameUI stops the test when the page is hidden
		BaseClass::SetVisible(state);
	}
};

class CSboKeyboardPage : public CSboSettingsPage
{
	DECLARE_CLASS_SIMPLE(CSboKeyboardPage, CSboSettingsPage);

public:
	CSboKeyboardPage(vgui2::Panel *pParent)
	    : BaseClass(pParent, "KeyboardPage")
	{
		m_pList = new CSboKeyList(this);
		m_pDefaults = new CSboBrowserButton(this, "Defaults", "Use defaults", "Defaults", CSboBrowserButton::KIND_NORMAL);
		m_pSetKey = new CSboBrowserButton(this, "SetKey", "Set key", "SetKey", CSboBrowserButton::KIND_NORMAL);
		m_pClear = new CSboBrowserButton(this, "Clear", "Clear key", "Clear", CSboBrowserButton::KIND_NORMAL);
	}
	CSboKeyList *m_pList;
	CSboBrowserButton *m_pDefaults, *m_pSetKey, *m_pClear;
	bool m_bConfirmDefaults = false;

	void ResetAll() override { m_pList->Load(false); }
	bool AnyChanged() override { return m_pList->m_bChanged; }
	void ApplyAll() override
	{
		if (m_pList->m_bChanged)
			m_pList->Apply();
	}
	void PerformLayout() override
	{
		vgui2::EditablePanel::PerformLayout();
		const int bh = S().M(30), gap = S().M(10);
		m_pList->SetBounds(0, 0, GetWide(), GetTall() - bh - gap);
		int x = 0;
		for (CSboBrowserButton *b : { m_pDefaults, m_pSetKey, m_pClear })
		{
			const int bw = b->GetContentWide();
			b->SetBounds(x, GetTall() - bh, bw, bh);
			x += bw + gap;
		}
	}
	void PaintBackground() override {}
	void Paint() override
	{
		if (m_bConfirmDefaults)
		{
			int x, y, w, h;
			m_pClear->GetBounds(x, y, w, h);
			DrawTextMid(S().body, COL_PING_MID, x + w + S().M(12), y, h,
			    L"Click again to load the default keys");
		}
	}
	void OnCommand(const char *command) override
	{
		if (!Q_stricmp(command, "Defaults"))
		{
			// GameUI asks first (its query box); here a second click
			if (!m_bConfirmDefaults)
			{
				m_bConfirmDefaults = true;
				Repaint();
				return;
			}
			m_bConfirmDefaults = false;
			m_pList->Load(true);
			NotifyChanged();
			Repaint();
		}
		else if (!Q_stricmp(command, "SetKey"))
			m_pList->StartCapture();
		else if (!Q_stricmp(command, "Clear"))
			m_pList->ClearSelected();
		else
			BaseClass::OnCommand(command);
	}
	MESSAGE_FUNC(OnSettingsChanged, "SettingsChanged") { NotifyChanged(); }
};
} // namespace

class CSboOptionsWindow : public CSboWindow
{
	DECLARE_CLASS_SIMPLE(CSboOptionsWindow, CSboWindow);

public:
	//! advanced: SBO's Advanced window (General, Chat, Version); else the Options window (GameUI's pages)
	CSboOptionsWindow(vgui2::Panel *pParent, bool advanced);
	//! Page (TAB_*) of a window tab, -1 if none
	int PageOfTab(int tab) const { return tab >= 0 && tab < (int)m_TabPages.size() ? m_TabPages[tab] : -1; }
	//! Window tab of a page (TAB_*), -1 if this window has no such page
	int TabOfPage(int page) const
	{
		for (int i = 0; i < (int)m_TabPages.size(); i++)
		{
			if (m_TabPages[i] == page)
				return i;
		}
		return -1;
	}
	void OnCommand(const char *command) override;
	void OnThink() override;

	MESSAGE_FUNC(OnSettingsChanged, "SettingsChanged")
	{
		UpdateSprayPreview();
		UpdateApply();
	}

protected:
	void OnTabChanged(int tab) override;
	void LayoutContent(int x, int y, int w, int h) override;

private:
	void BuildMultiplayer();
	void BuildMouse();
	void BuildAudio();
	void BuildVideo();
	void BuildVoice();
	void BuildContent();
	void BuildChat();
	void BuildGeneral();
	void BuildVersion();
	void ResetAll();
	void ApplyAll();
	void UpdateApply();
	void UpdateSprayPreview();
	void FillResolutions();

	CSboSettingsPage *m_pPages[TAB_COUNT] = {};
	std::vector<int> m_TabPages; //!< the page (TAB_*) of each window tab
	CSboBrowserButton *m_pOK = nullptr, *m_pApply = nullptr, *m_pCancel = nullptr;

	// Multiplayer: the spray and user.scr
	CSboCustomSetting *m_pLogo = nullptr, *m_pLogoColor = nullptr;
	CSboSprayPreview *m_pSprayPreview = nullptr;
	std::vector<std::string> m_Logos;
	int m_iLogoReset = -1, m_iLogoColorReset = 0;
	sbo_script::Description m_UserScr;
	std::vector<CSboCustomSetting *> m_UserRows;
	bool m_bUserScr = false;

	// Video
	CSboCustomSetting *m_pAspect = nullptr, *m_pResolution = nullptr, *m_pRenderer = nullptr, *m_pWindowed = nullptr;
	CSboCvarSlider *m_pGamma = nullptr;
	std::vector<vmode_t> m_Modes; //!< the resolution list's modes
	int m_iCurW = 0, m_iCurH = 0, m_iCurBpp = 32, m_iCurWindowed = 0;
	int m_iLastAspect = -1, m_iLastRenderer = -1;
	char m_szCurRenderer[128] = "";

	// Content
	CSboCustomSetting *m_pOriginalModels = nullptr, *m_pAddons = nullptr;
	// content lock (GameUI's CContentControlDialog)
	CSboBrowserEntry *m_pLockPassword = nullptr, *m_pLockPassword2 = nullptr;
	CSboCustomSetting *m_pLockReentry = nullptr; //!< the "Re-enter password" row (hidden while the lock is on)
	CSboBrowserButton *m_pLockButton = nullptr;
	int m_iLockMessage = -1;
	void UpdateContentLock(const char *message);
	void ContentLockCommand();
	int m_iHDModels = 0, m_iAddonsFolder = 0;

	// General
	vgui2::Label *m_pVersion = nullptr;
	int m_iUpdateState = -1; //!< the updater's state shown on the Version tab
	int m_iUpdateLine = -1;  //!< its text line there
};

CSboOptionsWindow::CSboOptionsWindow(vgui2::Panel *pParent, bool advanced)
    : BaseClass(pParent, advanced ? "SboAdvancedWindow" : "SboOptionsWindow", advanced ? "Advanced" : "Options",
          advanced ? "advanced" : "options", advanced ? 760 : 960, advanced ? 520 : 640)
{
	if (advanced)
		m_TabPages.assign(std::begin(s_AdvancedTabs), std::end(s_AdvancedTabs));
	else
		m_TabPages.assign(std::begin(s_OptionsTabs), std::end(s_OptionsTabs));
	for (int page : m_TabPages)
		AddTab(s_TabNames[page]);
	m_pOK = AddBottomButton("OK", "OK", true);
	m_pApply = AddBottomButton("Apply", "Apply", false);
	m_pCancel = AddBottomButton("Cancel", "Close", false);

	for (int page : m_TabPages)
	{
		switch (page)
		{
		case TAB_MULTIPLAYER: BuildMultiplayer(); break;
		case TAB_KEYBOARD: m_pPages[TAB_KEYBOARD] = new CSboKeyboardPage(this); break;
		case TAB_MOUSE: BuildMouse(); break;
		case TAB_AUDIO: BuildAudio(); break;
		case TAB_VIDEO: BuildVideo(); break;
		case TAB_VOICE: BuildVoice(); break;
		case TAB_CONTENT: BuildContent(); break;
		case TAB_CHAT: BuildChat(); break;
		case TAB_GENERAL: BuildGeneral(); break;
		case TAB_VERSION: BuildVersion(); break;
		}
	}
	ResetAll();
	OnTabChanged(0);
}

//! A color cvar (namecolor, hud_chat_color) as a swatch that opens the chat's color picker; written on Apply
static CSboCustomSetting *MakeColorSetting(CSboSettingsPage *p, const char *label, const char *cvar)
{
	CSboCustomSetting *s = new CSboCustomSetting();
	s->label = label;
	CSboColorField *field = new CSboColorField(p, "ColorField");
	field->SetLook(S().body, COL_TEXT, COL_BORDER, COL_BG_INPUT);
	field->AddActionSignalTarget(p);
	s->control = field;
	s->controlWide = 170;
	auto reset = std::make_shared<ChatColor>();
	const std::string name = cvar;
	s->onReset = [field, reset, name]() {
		*reset = ParseChatColor(CvarS(name.c_str()).c_str());
		field->SetColor(*reset);
	};
	s->onChanged = [field, reset]() { return field->GetColor() != *reset; };
	s->onApply = [field, reset, name]() {
		char v[16];
		CSboColorField::FormatValue(field->GetColor(), v, sizeof(v));
		sbo::CvarSetString(name.c_str(), v);
		*reset = field->GetColor();
	};
	return s;
}

void CSboOptionsWindow::BuildMultiplayer()
{
	CSboSettingsPage *p = m_pPages[TAB_MULTIPLAYER] = new CSboSettingsPage(this, "MultiplayerPage");
	p->AddCard("Player", 0);
	p->Add(new CSboCvarText(p, "Name", "name", 31));

	// name color: namecolor (userinfo), the same values as the chat color (hud_chat_color) and the same picker as the
	// chat line's: none, ^1..^8, any 24-bit color
	p->Add(MakeColorSetting(p, "Name color", "namecolor"))->tooltip = "The color of your name in chat, the scoreboard and the kill feed";

	// spray: GameUI's logo list and color, tempdecal.wad written on Apply
	{
		m_Logos = ListLogos();
		CSboCustomSetting *s = m_pLogo = new CSboCustomSetting();
		s->label = "Spray";
		CSboBrowserSelect *sel = new CSboBrowserSelect(p, "Logo");
		for (const std::string &l : m_Logos)
			sel->AddItem(l.c_str());
		sel->AddActionSignalTarget(p);
		s->control = sel;
		s->onReset = [this, sel]() {
			const std::string cur = CvarS("cl_logofile");
			m_iLogoReset = 0;
			for (int i = 0; i < (int)m_Logos.size(); i++)
			{
				if (!Q_stricmp(m_Logos[i].c_str(), cur.c_str()))
					m_iLogoReset = i;
			}
			sel->SetSelected(m_iLogoReset);
		};
		s->onChanged = [this, sel]() { return sel->GetSelected() != m_iLogoReset; };
		s->onApply = []() {}; // with the color (below)
		p->Add(s);

		CSboCustomSetting *c = m_pLogoColor = new CSboCustomSetting();
		c->label = "Spray color";
		CSboBrowserSelect *csel = new CSboBrowserSelect(p, "LogoColor");
		for (const LogoColor &lc : s_LogoColors)
			csel->AddItem(lc.label);
		csel->AddActionSignalTarget(p);
		c->control = csel;
		c->onReset = [this, csel]() {
			const std::string cur = CvarS("cl_logocolor");
			m_iLogoColorReset = 0;
			for (int i = 0; i < LOGO_COLOR_COUNT; i++)
			{
				if (!Q_stricmp(s_LogoColors[i].name, cur.c_str()))
					m_iLogoColorReset = i;
			}
			csel->SetSelected(m_iLogoColorReset);
		};
		c->onChanged = [this, csel]() { return csel->GetSelected() != m_iLogoColorReset; };
		c->onApply = [this, sel, csel]() {
			// GameUI: cl_logocolor, cl_logofile, then tempdecal.wad from the logo in that color
			const int ci = std::max(0, std::min(csel->GetSelected(), LOGO_COLOR_COUNT - 1));
			const int li = sel->GetSelected();
			char cmd[160];
			snprintf(cmd, sizeof(cmd), "cl_logocolor %s\n", s_LogoColors[ci].name);
			sbo::ExecClientCmd(cmd);
			if (li >= 0 && li < (int)m_Logos.size())
			{
				snprintf(cmd, sizeof(cmd), "cl_logofile %s\n", m_Logos[li].c_str());
				sbo::ExecClientCmd(cmd);
				WriteLogoWad(m_Logos[li], s_LogoColors[ci].r, s_LogoColors[ci].g, s_LogoColors[ci].b);
			}
			m_iLogoReset = li;
			m_iLogoColorReset = ci;
		};
		p->Add(c);
		CSboCustomSetting *pv = new CSboCustomSetting();
		pv->label = " ";
		m_pSprayPreview = new CSboSprayPreview(p);
		pv->control = m_pSprayPreview;
		pv->controlWide = 72;
		pv->rowTall = 76;
		pv->onReset = [this]() { UpdateSprayPreview(); };
		p->Add(pv);
		// the logo's apply runs through the color's (both change the file): a logo change alone applies it too
		s->onApply = [c]() { c->Apply(); };
	}


	// user.scr: GameUI's Multiplayer > Advanced (CMultiplayerAdvancedDialog)
	{
		// GameUI's own heading for these settings (its Multiplayer > Advanced dialog)
		std::string heading = LocalizedText("#GameUI_MultiplayerAdvanced");
		if (heading.empty() || heading[0] == '#')
			heading = "Multiplayer advanced";
		p->AddCard(heading.c_str(), 1);
	}
	std::string text;
	if (ReadFile("user.scr", text, 256 * 1024, "GAMECONFIG") && m_UserScr.Parse(text.data(), text.size(), sbo_script::Description::KIND_INFO))
	{
		m_bUserScr = true;
		// user_default.scr adds what user.scr lacks
		sbo_script::Description defaults;
		std::string dtext;
		if (ReadFile("user_default.scr", dtext, 256 * 1024) && defaults.Parse(dtext.data(), dtext.size(), sbo_script::Description::KIND_INFO))
		{
			for (const sbo_script::Object &o : defaults.objects)
			{
				if (sbo_script::Object *have = m_UserScr.Find(o.cvar.c_str()))
					have->prompt = o.prompt; // SBO's current wording (user.scr keeps the one it was written with)
				else if (m_UserScr.objects.size() < sbo_script::Description::MAX_OBJECTS)
					m_UserScr.objects.push_back(o);
			}
		}
		for (size_t i = 0; i < m_UserScr.objects.size(); i++)
		{
			sbo_script::Object *o = &m_UserScr.objects[i];
			// GameUI shows a "#token" prompt localized, any other as written (the file keeps the token)
			const std::string prompt = LocalizedText(o->prompt);
			CSboCustomSetting *s = new CSboCustomSetting();
			s->label = prompt; // a check box too: its prompt wraps in the label column
			if (o->type == sbo_script::TYPE_BOOL)
			{
				CSboBrowserCheck *chk = new CSboBrowserCheck(p, "UserBool", "");
				chk->AddActionSignalTarget(p);
				s->control = chk;
				s->rowTall = 26;
			}
			else if (o->type == sbo_script::TYPE_LIST)
			{
				CSboBrowserSelect *sel = new CSboBrowserSelect(p, "UserList");
				for (const sbo_script::ListItem &it : o->items)
					sel->AddItem(LocalizedText(it.label).c_str());
				sel->AddActionSignalTarget(p);
				s->control = sel;
			}
			else
			{
				CSboBrowserEntry *e = new CSboBrowserEntry(p, "UserText", "");
				e->SetMaximumCharCount(127);
				e->AddActionSignalTarget(p);
				s->control = e;
				s->controlWide = 120;
			}
			m_UserRows.push_back(s);
			p->Add(s);
		}
	}
	else
		p->AddText("user.scr not found");
}

void CSboOptionsWindow::BuildMouse()
{
	CSboSettingsPage *p = m_pPages[TAB_MOUSE] = new CSboSettingsPage(this, "MousePage");
	p->AddCard("Mouse", 0);
	p->Add(new CSboCvarSlider(p, "Sensitivity", "sensitivity", 0.2f, 20.0f, "%.1f"));
	p->Add(new CSboCvarNegate(p, "Reverse mouse", "m_pitch"));
	p->Add(new CSboCvarToggle(p, "Mouse filter", "m_filter"))->tooltip = "Smooths mouse movement by averaging it over two frames";
	p->Add(new CSboCvarToggle(p, "Raw mouse input", "m_rawinput"))->tooltip = "Reads the mouse directly, without the system's pointer settings";
	p->Add(new CSboCvarToggle(p, "Low latency mode", "gl_lowlatency"))->tooltip = "Less delay between your input and the screen";
	p->Add(new CSboCvarToggle(p, "Auto-aim", "sv_aim"));
}

void CSboOptionsWindow::BuildAudio()
{
	CSboSettingsPage *p = m_pPages[TAB_AUDIO] = new CSboSettingsPage(this, "AudioPage");
	p->AddCard("Audio", 0);
	p->Add(new CSboCvarSlider(p, "Sound effects", "volume", 0.0f, 2.0f, "%.2f"));
	p->Add(new CSboCvarSlider(p, "Music", "mp3volume", 0.0f, 1.0f, "%.2f"));
	p->Add(new CSboCommandChoice(p, "Sound quality", { "High", "Low" }, { "hisound 1\n", "hisound 0\n" },
	    []() { return CvarF("hisound") == 0.0f ? 1 : 0; }));
}

void CSboOptionsWindow::FillResolutions()
{
	CSboBrowserSelect *sel = static_cast<CSboBrowserSelect *>(m_pResolution->control);
	const bool wide = static_cast<CSboBrowserSelect *>(m_pAspect->control)->GetSelected() == 1;
	const bool software = static_cast<CSboBrowserSelect *>(m_pRenderer->control)->GetSelected() == 0;
	m_Modes.clear();
	sel->ClearItems();
	vmode_t *modes = nullptr;
	int count = 0;
	if (g_pGameUIFuncs)
		g_pGameUIFuncs->GetVideoModes(&modes, &count);
	if (!modes || count <= 0)
		return;
	// GameUI's PrepareResolutionList: 4:3 modes for Normal, all others for Wide; under 640x480 left out, over
	// 1599x1199 for the software renderer (only when there is more than one mode)
	for (int i = 0; i < count && i < 512; i++)
	{
		const vmode_t &m = modes[i];
		if ((m.iWidth * 3 == m.iHeight * 4) == wide)
			continue;
		if (count > 1 && (m.iWidth < 640 || m.iHeight < 480))
			continue;
		if (count > 1 && software && (m.iWidth > 1599 || m.iHeight > 1199))
			continue;
		bool dup = false;
		for (const vmode_t &o : m_Modes)
			dup = dup || (o.iWidth == m.iWidth && o.iHeight == m.iHeight);
		if (dup)
			continue;
		m_Modes.push_back(m);
		char buf[32];
		snprintf(buf, sizeof(buf), "%i x %i", m.iWidth, m.iHeight);
		sel->AddItem(buf);
	}
	int pick = 0;
	for (int i = 0; i < (int)m_Modes.size(); i++)
	{
		if (m_Modes[i].iWidth == m_iCurW && m_Modes[i].iHeight == m_iCurH)
			pick = i;
	}
	sel->SetSelected(pick);
}

void CSboOptionsWindow::BuildVideo()
{
	CSboSettingsPage *p = m_pPages[TAB_VIDEO] = new CSboSettingsPage(this, "VideoPage");
	p->AddCard("Display", 0);
	auto selSetting = [p](const char *label, std::initializer_list<const char *> items) {
		CSboCustomSetting *s = new CSboCustomSetting();
		s->label = label;
		CSboBrowserSelect *sel = new CSboBrowserSelect(p, "VideoSelect");
		for (const char *i : items)
			sel->AddItem(i);
		sel->AddActionSignalTarget(p);
		s->control = sel;
		return s;
	};
	m_pRenderer = selSetting("Renderer", { "Software", "OpenGL" });
	m_pAspect = selSetting("Aspect ratio", { "Normal (4:3)", "Widescreen" });
	m_pResolution = selSetting("Resolution", {});
	m_pWindowed = new CSboCustomSetting();
	{
		CSboBrowserCheck *chk = new CSboBrowserCheck(p, "Windowed", "Run in a window");
		chk->AddActionSignalTarget(p);
		m_pWindowed->control = chk;
		m_pWindowed->rowTall = 26;
	}
	// the four apply together (ApplyVidSettings, from the window's Apply)
	auto pendingDiffers = [this]() {
		const int ri = static_cast<CSboBrowserSelect *>(m_pResolution->control)->GetSelected();
		const bool software = static_cast<CSboBrowserSelect *>(m_pRenderer->control)->GetSelected() == 0;
		const bool windowed = static_cast<CSboBrowserCheck *>(m_pWindowed->control)->IsChecked();
		const bool resChanged = ri >= 0 && ri < (int)m_Modes.size() && (m_Modes[ri].iWidth != m_iCurW || m_Modes[ri].iHeight != m_iCurH);
		const bool curSoftware = !Q_stricmp(m_szCurRenderer, "software");
		return resChanged || software != curSoftware || windowed != (m_iCurWindowed != 0);
	};
	for (CSboCustomSetting *s : { m_pRenderer, m_pAspect, m_pResolution, m_pWindowed })
	{
		s->onChanged = pendingDiffers;
		s->onApply = []() {}; // the window's ApplyAll sends the video mode once
	}
	m_pRenderer->onReset = [this]() {
		CurrentRenderer(m_szCurRenderer, sizeof(m_szCurRenderer), &m_iCurWindowed, &m_iHDModels, &m_iAddonsFolder);
		if (g_pGameUIFuncs)
			g_pGameUIFuncs->GetCurrentVideoMode(&m_iCurW, &m_iCurH, &m_iCurBpp);
		static_cast<CSboBrowserSelect *>(m_pRenderer->control)->SetSelected(!Q_stricmp(m_szCurRenderer, "software") ? 0 : 1);
		static_cast<CSboBrowserSelect *>(m_pAspect->control)->SetSelected(m_iCurW * 3 == m_iCurH * 4 ? 0 : 1);
		static_cast<CSboBrowserCheck *>(m_pWindowed->control)->SetChecked(m_iCurWindowed != 0);
		FillResolutions();
		if (static_cast<CSboBrowserSelect *>(m_pResolution->control)->GetItemCount() == 0)
		{
			// the chosen aspect has no modes: the other one (GameUI flips it)
			CSboBrowserSelect *a = static_cast<CSboBrowserSelect *>(m_pAspect->control);
			a->SetSelected(1 - a->GetSelected());
			FillResolutions();
		}
	};
	p->Add(m_pRenderer);
	p->Add(m_pAspect);
	p->Add(m_pResolution);
	p->Add(m_pWindowed);
	p->AddText("Renderer, resolution and window apply with a video restart");

	p->AddCard("Picture", 1);
	p->Add(new CSboCvarSlider(p, "Brightness", "brightness", 0.0f, 2.0f, "%.2f"));
	m_pGamma = static_cast<CSboCvarSlider *>(p->Add(new CSboCvarSlider(p, "Gamma", "gamma", 1.0f, 3.0f, "%.2f")));
	p->Add(new CSboCvarToggle(p, "Wait for vertical sync", "gl_vsync"))->tooltip = "Matches the frame rate to the monitor: no tearing, a little more delay";
	p->Add(new CSboCvarToggle(p, "Widescreen field of view", "gl_widescreen_yfov"))->tooltip = "Keeps the 4:3 height on wide screens, so you see more at the sides";
	p->Add(new CSboCvarToggle(p, "Prefer texture filtering", "r_prefertexturefiltering"))->tooltip = "Smooth textures instead of sharp pixels";
	p->Add(new CSboCvarToggle(p, "Use shaders", "gl_use_shaders"));
}

void CSboOptionsWindow::BuildVoice()
{
	CSboVoicePage *p = new CSboVoicePage(this);
	m_pPages[TAB_VOICE] = p;
	p->AddCard("Voice", 0);
	p->Add(new CSboCvarToggle(p, "Enable voice", "voice_modenable"));
	p->Add(new CSboCvarSlider(p, "Receive volume", "voice_scale", 0.0f, 1.0f, "%.2f"))->tooltip = "How loud other players' voices are";
	VoiceTweak *v = Voice();
	{
		CSboCustomSetting *s = p->m_pMicVolume = new CSboCustomSetting();
		s->label = "Transmit volume";
		s->control = SboMakeValueSlider(p, 0.0f, 1.0f, "%.2f");
		s->rowTall = 28;
		CSboSlider *sl = SboValueSliderOf(s->control);
		auto reset = std::make_shared<float>(0.0f);
		s->onReset = [sl, reset]() {
			VoiceTweak *vt = Voice();
			*reset = vt ? vt->GetControlFloat(VOICE_MIC_VOLUME) : 0.0f;
			sl->SetValue(*reset);
			*reset = sl->GetValue();
			sl->GetParent()->Repaint();
		};
		s->onChanged = [sl, reset]() { return sl->GetValue() != *reset; };
		s->onApply = [sl, reset]() {
			if (VoiceTweak *vt = Voice())
				vt->SetControlFloat(VOICE_MIC_VOLUME, sl->GetValue());
			*reset = sl->GetValue();
		};
		p->Add(s);
	}
	{
		CSboCustomSetting *s = p->m_pBoost = new CSboCustomSetting();
		s->tooltip = "Makes your microphone louder";
		CSboBrowserCheck *chk = new CSboBrowserCheck(p, "MicBoost", "Boost microphone gain");
		chk->AddActionSignalTarget(p);
		s->control = chk;
		s->rowTall = 26;
		auto reset = std::make_shared<bool>(false);
		s->onReset = [chk, reset]() {
			VoiceTweak *vt = Voice();
			*reset = vt && vt->GetControlFloat(VOICE_MIC_BOOST) != 0.0f;
			chk->SetChecked(*reset);
		};
		s->onChanged = [chk, reset]() { return chk->IsChecked() != *reset; };
		s->onApply = [chk, reset]() {
			if (VoiceTweak *vt = Voice())
				vt->SetControlFloat(VOICE_MIC_BOOST, chk->IsChecked() ? 1.0f : 0.0f);
			*reset = chk->IsChecked();
		};
		p->Add(s);
	}
	p->m_pTest = p->AddButton("Test microphone", "TestMic");
	// the engine's automatic gain for the voices heard (CAutoGain::Reset(128, voice_maxgain, voice_avggain,
	// voice_scale) when a speaker starts): not archived by the engine, kept in voicegain.vdf (SboVoiceGain_*)
	p->m_pMaxGain = new CSboCvarSlider(p, "Max voice boost", "voice_maxgain", 1.0f, 10.0f, "%.0fx");
	p->m_pMaxGain->tooltip = "How much quiet microphones are boosted";
	p->Add(p->m_pMaxGain);
	p->m_pAvgGain = new CSboCvarSlider(p, "Voice leveling", "voice_avggain", 0.0f, 1.0f, "percent");
	p->m_pAvgGain->tooltip = "How strongly loud and quiet voices are evened out";
	p->Add(p->m_pAvgGain);
	p->AddButton("Default", "VoiceGainDefault");
	if (!v)
	{
		p->AddText("Voice controls not available");
		p->m_pMicVolume->control->SetEnabled(false);
		p->m_pBoost->control->SetEnabled(false);
		p->m_pTest->SetEnabled(false);
	}
}

void CSboOptionsWindow::BuildContent()
{
	CSboSettingsPage *p = m_pPages[TAB_CONTENT] = new CSboSettingsPage(this, "ContentPage");
	p->AddCard("Content", 0);
	// r_detailtextures: the engine's detail textures (maps/<map>_detail.txt), applied at once
	p->Add(new CSboCvarToggle(p, "Detail textures", "r_detailtextures"))->tooltip = "Extra surface detail on maps that come with detail textures";
	m_pOriginalModels = new CSboCustomSetting();
	m_pOriginalModels->tooltip = "Uses Half-Life's original models instead of the HD models";
	{
		CSboBrowserCheck *chk = new CSboBrowserCheck(p, "OriginalModels", "Use the original Half-Life models");
		chk->AddActionSignalTarget(p);
		m_pOriginalModels->control = chk;
		m_pOriginalModels->rowTall = 26;
		m_pOriginalModels->onReset = [this, chk]() { chk->SetChecked(m_iHDModels == 0); };
		m_pOriginalModels->onChanged = [this, chk]() { return (!chk->IsChecked()) != (m_iHDModels != 0); };
		m_pOriginalModels->onApply = []() {}; // ApplyAll (one _restart for both)
	}
	m_pAddons = new CSboCustomSetting();
	m_pAddons->tooltip = "Also loads files from the syndicate_addon folder";
	{
		CSboBrowserCheck *chk = new CSboBrowserCheck(p, "Addons", "Allow custom addon content");
		chk->AddActionSignalTarget(p);
		m_pAddons->control = chk;
		m_pAddons->rowTall = 26;
		m_pAddons->onReset = [this, chk]() { chk->SetChecked(m_iAddonsFolder != 0); };
		m_pAddons->onChanged = [this, chk]() { return chk->IsChecked() != (m_iAddonsFolder == 1); };
		m_pAddons->onApply = []() {};
	}
	p->Add(m_pOriginalModels);
	p->Add(m_pAddons);
	p->AddText("Models and addon content apply with a game restart");

	// GameUI's Content lock (CContentControlDialog): a password turns blood and gore off (violence_* 0); the same
	// password turns them on again. The password's MD5 (hex) is kept where GameUI keeps it, so either dialog unlocks.
	p->AddCard("Content lock", 1);
	p->AddText("Password lock for low violence: no blood or gore");
	auto entry = [p](const char *label, CSboBrowserEntry *&pEntry) -> CSboCustomSetting * {
		CSboCustomSetting *s = new CSboCustomSetting();
		s->label = label;
		pEntry = new CSboBrowserEntry(p, "LockPassword", "");
		pEntry->SetTextHidden(true);
		pEntry->SetMaximumCharCount(63);
		s->control = pEntry;
		s->controlWide = 160;
		CSboBrowserEntry *e = pEntry;
		s->onReset = [e]() { e->SetText(""); };
		p->Add(s); // never "changed": the lock applies from its own button
		return s;
	};
	entry("Password", m_pLockPassword);
	m_pLockReentry = entry("Re-enter password", m_pLockPassword2);
	m_pLockButton = p->AddButton("Lock", "ContentLock", "Lock is off");
	m_iLockMessage = p->AddText("");
	UpdateContentLock(nullptr);
}

namespace
{
const char *const CONTENT_LOCK_KEY = "Software\\Valve\\Half-Life\\Settings\\User Token 2";

//! GameUI's HashPassword: MD5 of the password as 32 lower-case hex digits
std::string ContentLockHash(const char *password)
{
	MD5Context_t ctx;
	unsigned char digest[MD5_DIGEST_LENGTH];
	MD5Init(&ctx);
	MD5Update(&ctx, (const unsigned char *)password, (unsigned int)strlen(password));
	MD5Final(digest, &ctx);
	char hex[MD5_DIGEST_LENGTH * 2 + 1];
	for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
		snprintf(hex + i * 2, 3, "%02x", digest[i]);
	return hex;
}

// GameUI keeps the token in the dialog too (m_szGorePW) and ignores whether the registry write worked: where the
// engine has no registry (Linux), the lock lasts until the game is closed
std::string s_ContentLockToken;
bool s_bContentLockRead = false;

std::string ContentLockToken()
{
	if (!s_bContentLockRead)
	{
		s_bContentLockRead = true;
		char token[128] = "";
		if (vgui2::system() && vgui2::system()->GetRegistryString(CONTENT_LOCK_KEY, token, sizeof(token)))
			s_ContentLockToken = token;
	}
	return s_ContentLockToken;
}

void WriteContentLockToken(const std::string &token)
{
	s_ContentLockToken = token;
	s_bContentLockRead = true;
	if (vgui2::system())
		vgui2::system()->SetRegistryString(CONTENT_LOCK_KEY, token.c_str());
}

//! GameUI's EnablePassword / DisablePassword: the four violence cvars, 0 locked, 1 unlocked
void ContentLockViolence(bool locked)
{
	for (const char *cvar : { "violence_hblood", "violence_hgibs", "violence_ablood", "violence_agibs" })
		sbo::CvarSetString(cvar, locked ? "0" : "1");
}
} // namespace

void CSboOptionsWindow::UpdateContentLock(const char *message)
{
	if (!m_pLockButton)
		return;
	CSboSettingsPage *p = m_pPages[TAB_CONTENT];
	const bool locked = !ContentLockToken().empty();
	m_pLockButton->SetText(locked ? "Unlock" : "Lock");
	p->SetButtonLabel(m_pLockButton, locked ? "Lock is on" : "Lock is off");
	// unlocking needs the password once (GameUI's "Please enter password"); the re-entry row is hidden while the lock
	// is on, as GameUI's UpdateContentControlStatus hides its PasswordReentryPrompt and Password2
	m_pLockReentry->hidden = locked;
	m_pLockPassword2->SetVisible(!locked);
	m_pLockPassword->SetText("");
	m_pLockPassword2->SetText("");
	p->SetText(m_iLockMessage, message ? message : (locked ? "Enter the password to unlock" : "Enter a password twice to lock"));
	p->InvalidateLayout();
}

void CSboOptionsWindow::ContentLockCommand()
{
	char pw[64] = "", pw2[64] = "";
	m_pLockPassword->GetText(pw, sizeof(pw));
	m_pLockPassword2->GetText(pw2, sizeof(pw2));
	const std::string token = ContentLockToken();
	if (!token.empty())
	{
		if (Q_stricmp(ContentLockHash(pw).c_str(), token.c_str()))
		{
			UpdateContentLock("Incorrect password");
			return;
		}
		WriteContentLockToken("");
		ContentLockViolence(false);
		UpdateContentLock("Unlocked");
		return;
	}
	if (!pw[0])
	{
		UpdateContentLock("Enter a password");
		return;
	}
	if (strcmp(pw, pw2))
	{
		UpdateContentLock("The passwords do not match");
		return;
	}
	WriteContentLockToken(ContentLockHash(pw));
	ContentLockViolence(true);
	UpdateContentLock("Locked");
}

void CSboOptionsWindow::BuildChat()
{
	CSboSettingsPage *p = m_pPages[TAB_CHAT] = new CSboSettingsPage(this, "ChatPage");
	p->AddCard("Chat", 0);
	std::vector<float> sizes;
	std::vector<std::string> sizeLabels;
	for (int i = 14; i <= 34; i++)
	{
		sizes.push_back((float)i);
		char buf[32];
		snprintf(buf, sizeof(buf), i == 17 ? "%d (default)" : "%d", i);
		sizeLabels.push_back(buf);
	}
	p->Add(new CSboCvarChoice(p, "Text size", "hud_chat_fontsize", sizes, sizeLabels))->tooltip = "The size of the chat text";
	p->Add(MakeColorSetting(p, "Chat color", "hud_chat_color"))->tooltip = "The color of your chat text that everyone sees";
	p->Add(new CSboCvarToggle(p, "Name and chat colors", "hud_colortext", 1.0f, 2.0f))->tooltip = "Shows the colors in names and chat; off removes the color codes";
	p->Add(new CSboCvarToggle(p, "Message sound", "hud_saytext_sound"))->tooltip = "Plays a sound for each new chat message";
	std::vector<float> times = { 5, 8, 10, 12, 15, 20, 30, 60 };
	std::vector<std::string> timeLabels;
	for (float t : times)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), t == 12 ? "%d seconds (default)" : "%d seconds", (int)t);
		timeLabels.push_back(buf);
	}
	p->Add(new CSboCvarChoice(p, "Message time", "hud_saytext_time", times, timeLabels))->tooltip = "How long chat messages stay on screen";
}

void CSboOptionsWindow::BuildGeneral()
{
	CSboSettingsPage *p = m_pPages[TAB_GENERAL] = new CSboSettingsPage(this, "GeneralPage");
	p->AddCard("Discord", 0);
	p->Add(new CSboCvarToggle(p, "Discord Rich Presence", "cl_discord_rpc"));
	p->AddText("Shows your server and map in your Discord profile");
	// cl_righthand (weapon_hand.cpp, docs/WEAPON_HAND.md): 1 = the first-person weapon in the left hand (as the old
	// Advanced dialog)
	p->AddCard("Weapons", 0);
	p->Add(new CSboCvarToggle(p, "Left-handed weapons", "cl_righthand"))->tooltip = "Holds your weapon on the left side of the screen";
	p->AddCard("Menus", 0);
	p->Add(new CSboCvarToggle(p, "New GameUI menus", "cl_experimental_menus"));
	p->AddText("Use the new server browser, joining screen and settings windows");
	p->Add(new CSboCvarToggle(p, "New in-game menus", "cl_new_ingame_menus"));
	p->AddText("New loadout, MOTD and map info windows");
	// the updater (docs/UPDATER.md)
	p->AddCard("Updates", 0);
	p->Add(new CSboCvarToggle(p, "Check for updates at start", "cl_check_for_updates"))->tooltip =
	    "Looks for a new version when the game starts and offers to install it";
}

void CSboOptionsWindow::BuildVersion()
{
	CSboSettingsPage *p = m_pPages[TAB_VERSION] = new CSboSettingsPage(this, "VersionPage");
	p->AddCard("Syndicate Black Ops", 0, true);
	char buf[192];
	snprintf(buf, sizeof(buf), "Version: %s", SboVersion_Full());
	p->AddText(buf);
	m_iUpdateLine = p->AddText(sbo_update::StatusText().c_str());
	m_iUpdateState = (int)sbo_update::GetState();
	p->AddButton("Check for updates", "CheckUpdates");
	p->AddLink(SBO_CREDITS_TEXT, SBO_REPO_LINK_TEXT, SBO_REPO_URL);
}

void CSboOptionsWindow::ResetAll()
{
	// video first: the content tab's state comes from the same engine call
	if (m_pRenderer)
		m_pRenderer->Reset();
	for (CSboSettingsPage *p : m_pPages)
	{
		if (p)
			p->ResetAll();
	}
	// user.scr: the cvars' current values over the file's (GameUI's TransferCurrentValues)
	for (size_t i = 0; i < m_UserRows.size() && i < m_UserScr.objects.size(); i++)
	{
		sbo_script::Object &o = m_UserScr.objects[i];
		std::string cur = o.setInfo ? sbo::GetLocalUserInfo(o.cvar.c_str()) : CvarS(o.cvar.c_str());
		if (!cur.empty())
			o.defValue = o.curValue = cur;
		CSboCustomSetting *s = m_UserRows[i];
		auto reset = std::make_shared<std::string>();
		if (o.type == sbo_script::TYPE_BOOL)
		{
			CSboBrowserCheck *chk = static_cast<CSboBrowserCheck *>(s->control);
			chk->SetChecked(atof(o.defValue.c_str()) != 0.0);
			const bool r = chk->IsChecked();
			s->onChanged = [chk, r]() { return chk->IsChecked() != r; };
		}
		else if (o.type == sbo_script::TYPE_LIST)
		{
			CSboBrowserSelect *sel = static_cast<CSboBrowserSelect *>(s->control);
			const int row = o.ListRow();
			sel->SetSelected(std::max(0, row));
			s->onChanged = [sel, row]() { return sel->GetSelected() != std::max(0, row); };
		}
		else
		{
			CSboBrowserEntry *e = static_cast<CSboBrowserEntry *>(s->control);
			const std::string r = o.type == sbo_script::TYPE_NUMBER ? sbo_script::NumberText(o.defValue) : o.defValue;
			e->SetText(r.c_str());
			s->onChanged = [e, r]() {
				char buf[256];
				e->GetText(buf, sizeof(buf));
				return r != buf;
			};
		}
	}
	UpdateApply();
}

void CSboOptionsWindow::ApplyAll()
{
	// video: GameUI's ApplyVidSettings (gamma's own slider applies with the page; a gamma change restarts the video too)
	const bool gammaChanged = m_pGamma && m_pGamma->Changed();
	const bool vidChanged = m_pResolution && m_pResolution->Changed();
	for (CSboSettingsPage *p : m_pPages)
	{
		if (p)
			p->ApplyAll();
	}
	if (vidChanged || gammaChanged)
	{
		const int ri = static_cast<CSboBrowserSelect *>(m_pResolution->control)->GetSelected();
		int w = m_iCurW, h = m_iCurH;
		if (ri >= 0 && ri < (int)m_Modes.size())
		{
			w = m_Modes[ri].iWidth;
			h = m_Modes[ri].iHeight;
		}
		const bool software = static_cast<CSboBrowserSelect *>(m_pRenderer->control)->GetSelected() == 0;
		const bool windowed = static_cast<CSboBrowserCheck *>(m_pWindowed->control)->IsChecked();
		const bool sameRenderer = software == !Q_stricmp(m_szCurRenderer, "software");
		const int bpp = sameRenderer && m_iCurBpp == 16 ? 16 : 32;
		char cmd[128];
		snprintf(cmd, sizeof(cmd), "_setvideomode %i %i %i\n", w, h, bpp);
		sbo::ExecClientCmd(cmd);
		snprintf(cmd, sizeof(cmd), "_setrenderer %s %s\n", software ? "software" : "gl", windowed ? "windowed" : "fullscreen");
		sbo::ExecClientCmd(cmd);
		sbo::ExecClientCmd("_restart\n");
	}
	// content: GameUI's COptionsSubAdvanced
	bool restart = false;
	CSboBrowserCheck *orig = m_pOriginalModels ? static_cast<CSboBrowserCheck *>(m_pOriginalModels->control) : nullptr;
	if (orig && (!orig->IsChecked()) != (m_iHDModels != 0))
	{
		char cmd[64];
		snprintf(cmd, sizeof(cmd), "_sethdmodels %d\n", orig->IsChecked() ? 0 : 1);
		sbo::ExecClientCmd(cmd);
		m_iHDModels = orig->IsChecked() ? 0 : 1;
		restart = true;
	}
	CSboBrowserCheck *addons = m_pAddons ? static_cast<CSboBrowserCheck *>(m_pAddons->control) : nullptr;
	if (addons && addons->IsChecked() != (m_iAddonsFolder == 1))
	{
		char cmd[64];
		snprintf(cmd, sizeof(cmd), "_setaddons_folder %d\n", addons->IsChecked() ? 1 : 0);
		sbo::ExecClientCmd(cmd);
		m_iAddonsFolder = addons->IsChecked() ? 1 : 0;
		restart = true;
	}
	if (restart && !(vidChanged || gammaChanged))
		sbo::ExecClientCmd("_restart\n");

	// user.scr: GameUI's CMultiplayerAdvancedDialog (the values to the engine, then the file)
	if (m_bUserScr)
	{
		bool any = false;
		for (size_t i = 0; i < m_UserRows.size() && i < m_UserScr.objects.size(); i++)
		{
			sbo_script::Object &o = m_UserScr.objects[i];
			CSboCustomSetting *s = m_UserRows[i];
			any = any || s->Changed();
			if (o.type == sbo_script::TYPE_BOOL)
				o.SetCurValue(static_cast<CSboBrowserCheck *>(s->control)->IsChecked() ? "1" : "0");
			else if (o.type == sbo_script::TYPE_LIST)
			{
				const int row = static_cast<CSboBrowserSelect *>(s->control)->GetSelected();
				o.SetCurValue(row >= 0 && row < (int)o.items.size() ? o.items[row].value : o.defValue);
			}
			else
			{
				char buf[256];
				static_cast<CSboBrowserEntry *>(s->control)->GetText(buf, sizeof(buf));
				o.SetCurValue(buf);
			}
		}
		if (any)
		{
			for (const sbo_script::Object &o : m_UserScr.objects)
				sbo::ExecClientCmd(o.ConfigCommand().c_str());
			const std::string out = m_UserScr.Write(sbo_script::FileTime((long long)time(nullptr), sbo_script::Description::KIND_INFO));
			WriteFile("user.scr", out, "GAMECONFIG");
		}
	}
	ResetAll();
}

void CSboOptionsWindow::UpdateSprayPreview()
{
	if (!m_pSprayPreview || !m_pLogo || !m_pLogoColor)
		return;
	// the spray chosen in the lists (as GameUI's preview); the player's own animated spray while they are unchanged
	if (!m_pLogo->Changed() && !m_pLogoColor->Changed() && m_pSprayPreview->ShowCurrentIfAnimated())
		return;
	const int li = static_cast<CSboBrowserSelect *>(m_pLogo->control)->GetSelected();
	const int ci = std::max(0, std::min(static_cast<CSboBrowserSelect *>(m_pLogoColor->control)->GetSelected(), LOGO_COLOR_COUNT - 1));
	if (li >= 0 && li < (int)m_Logos.size())
		m_pSprayPreview->ShowLogo(m_Logos[li], s_LogoColors[ci].r, s_LogoColors[ci].g, s_LogoColors[ci].b);
}

void CSboOptionsWindow::UpdateApply()
{
	bool any = false;
	for (CSboSettingsPage *p : m_pPages)
		any = any || (p && p->AnyChanged());
	m_pApply->SetEnabled(any);
}

void CSboOptionsWindow::OnTabChanged(int tab)
{
	for (int i = 0; i < TAB_COUNT; i++)
	{
		if (m_pPages[i])
			m_pPages[i]->SetVisible(tab >= 0 && tab < (int)m_TabPages.size() && m_TabPages[tab] == i);
	}
}

void CSboOptionsWindow::LayoutContent(int x, int y, int w, int h)
{
	for (CSboSettingsPage *p : m_pPages)
	{
		if (p)
		{
			p->SetBounds(x, y, w, h);
			p->InvalidateLayout();
		}
	}
}

void CSboOptionsWindow::OnThink()
{
	BaseClass::OnThink();
	// the video page: aspect / renderer change the resolution list
	CSboBrowserSelect *a = m_pAspect ? static_cast<CSboBrowserSelect *>(m_pAspect->control) : nullptr;
	CSboBrowserSelect *r = m_pRenderer ? static_cast<CSboBrowserSelect *>(m_pRenderer->control) : nullptr;
	if (a && r && (a->GetSelected() != m_iLastAspect || r->GetSelected() != m_iLastRenderer))
	{
		const bool first = m_iLastAspect < 0;
		m_iLastAspect = a->GetSelected();
		m_iLastRenderer = r->GetSelected();
		if (!first)
		{
			FillResolutions();
			UpdateApply();
		}
	}
	if (m_pPages[TAB_VERSION] && m_iUpdateLine >= 0 && (int)sbo_update::GetState() != m_iUpdateState)
	{
		m_iUpdateState = (int)sbo_update::GetState();
		m_pPages[TAB_VERSION]->SetText(m_iUpdateLine, sbo_update::StatusText().c_str());
	}
}

void CSboOptionsWindow::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "OK"))
	{
		ApplyAll();
		SboVoiceGain_Save();
		Close();
		return;
	}
	if (!Q_stricmp(command, "Apply"))
	{
		ApplyAll();
		SboVoiceGain_Save();
		return;
	}
	if (!Q_stricmp(command, "ContentLock"))
	{
		ContentLockCommand();
		return;
	}
	if (!Q_stricmp(command, "CheckUpdates"))
	{
		sbo_update::CheckForUpdates();
		return;
	}
	BaseClass::OnCommand(command);
}

//-----------------------------------------------------------------------------
// Commands
//-----------------------------------------------------------------------------
static vgui2::DHANDLE<CSboOptionsWindow> s_hOptions, s_hAdvanced;
static bool s_bOpenNextFrame = false;
static bool s_bOpenAdvanced = false;
static int s_iOpenPage = -1;
static int s_iDeferredAdvancedPage = -1; //!< VGui2_MenusStyle_Frame: the Advanced window opens after the Options window

//! Opens the Options window (advanced false) or the Advanced window on the next frame, on the page named (or its first)
static void OpenWindowNextFrame(bool advanced, const char *pageName)
{
	s_bOpenAdvanced = advanced;
	s_iOpenPage = -1;
	for (int i = 0; pageName && i < TAB_COUNT; i++)
	{
		if (!Q_stricmp(pageName, s_TabNames[i]))
			s_iOpenPage = i;
	}
	if (g_pBaseUI)
		g_pBaseUI->ActivateGameUI();
	s_bOpenNextFrame = true;
}

void VGui2_OpenAdvancedWindow()
{
	OpenWindowNextFrame(true, nullptr);
}

// gameui_options [page]: the game menu's Options entry ("engine gameui_options", which hides GameUI: shown again and
// the window opened on the next frame, as gameui_serverbrowser). A page of the Advanced window (General, Chat,
// Version) opens that window. cl_experimental_menus 0: GameUI's Options dialog.
static void OptionsWindow_f()
{
	if (!g_pGameUIViewport)
		return;
	if (!SboExperimentalMenus())
	{
		if (g_pBaseUI)
			g_pBaseUI->ActivateGameUI();
		SboOpenGameUIDialog("OpenOptionsDialog");
		return;
	}
	const char *page = sbo::Cmd_Argc() > 1 ? sbo::Cmd_Argv(1) : nullptr;
	bool advanced = false;
	for (int p : s_AdvancedTabs)
		advanced = advanced || (page && !Q_stricmp(page, s_TabNames[p]));
	OpenWindowNextFrame(advanced, page);
}

void VGui2_OptionsWindow_Frame()
{
	if (!s_bOpenNextFrame || !g_pGameUIViewport)
		return;
	s_bOpenNextFrame = false;
	vgui2::DHANDLE<CSboOptionsWindow> &h = s_bOpenAdvanced ? s_hAdvanced : s_hOptions;
	if (!h.Get())
		h = new CSboOptionsWindow(g_pGameUIViewport, s_bOpenAdvanced);
	h->Open();
	const int tab = s_iOpenPage >= 0 ? h->TabOfPage(s_iOpenPage) : 0;
	h->SetTab(tab >= 0 ? tab : 0);
	if (s_iDeferredAdvancedPage >= 0)
	{
		OpenWindowNextFrame(true, s_TabNames[s_iDeferredAdvancedPage]);
		s_iDeferredAdvancedPage = -1;
	}
}

namespace
{
//! A panel named name under panel (depth-limited), 0 if none
vgui2::VPANEL FindNamedPanel(vgui2::VPANEL panel, const char *name, int depth)
{
	if (!panel || depth > 4)
		return 0;
	const char *n = vgui2::ipanel()->GetName(panel);
	if (n && !Q_stricmp(n, name))
		return panel;
	for (int i = 0; i < vgui2::ipanel()->GetChildCount(panel); i++)
	{
		if (vgui2::VPANEL found = FindNamedPanel(vgui2::ipanel()->GetChild(panel, i), name, depth + 1))
			return found;
	}
	return 0;
}

//! GameUI's dialog of that panel name ("OptionsDialog", "CServerBrowserDialog", "CreateMultiplayerGameDialog"; names
//! from vgui2_dumptree on HL25), if it is shown
vgui2::VPANEL FindShownGameUIDialog(const char *name)
{
	if (!g_pEngineVGui)
		return 0;
	vgui2::VPANEL p = FindNamedPanel(g_pEngineVGui->GetPanel(PANEL_GAMEUIDLL), name, 0);
	return p && vgui2::ipanel()->IsVisible(p) ? p : 0;
}

//! The page (TAB_*) GameUI's Options dialog shows: its visible COptionsSub<Name> page, -1 if none of ours
int GameUIOptionsPage(vgui2::VPANEL panel, int depth)
{
	if (!panel || depth > 3)
		return -1;
	const char *cls = vgui2::ipanel()->GetClassName(panel);
	if (depth > 0 && cls && !Q_strncmp(cls, "COptionsSub", 11) && vgui2::ipanel()->IsVisible(panel))
	{
		for (int i = 0; i < TAB_COUNT; i++)
		{
			if (!Q_stricmp(cls + 11, s_TabNames[i]))
				return i;
		}
		return -1;
	}
	for (int i = 0; i < vgui2::ipanel()->GetChildCount(panel); i++)
	{
		const int page = GameUIOptionsPage(vgui2::ipanel()->GetChild(panel, i), depth + 1);
		if (page >= 0)
			return page;
	}
	return -1;
}

void CloseGameUIDialog(vgui2::VPANEL dialog)
{
	vgui2::ivgui()->PostMessage(dialog, new KeyValues("Command", "command", "Close"), 0);
}

//! Closes SBO's new-style window if it is open; its page (TAB_*) or -1 if it was not open
int CloseOptionsWindow(vgui2::DHANDLE<CSboOptionsWindow> &h)
{
	CSboOptionsWindow *p = h.Get();
	if (!p || !p->IsVisible())
		return -1;
	const int page = p->PageOfTab(p->GetTab());
	h = nullptr; // it deletes itself on close; the next open makes a new one
	p->Close();
	return page >= 0 ? page : 0;
}
} // namespace

void VGui2_MenusStyle_Frame()
{
	static int s_iStyle = -1;
	const int style = SboExperimentalMenus() ? 1 : 0;
	if (s_iStyle < 0 || style == s_iStyle)
	{
		s_iStyle = style;
		return;
	}
	s_iStyle = style;
	VGui2_Updater_MenusStyleChanged(); // the update window opens again in the new look
	if (style)
	{
		// to the new menus: GameUI's dialogs and SBO's old Advanced dialog close, SBO's windows open in their place
		if (vgui2::VPANEL d = FindShownGameUIDialog("CServerBrowserDialog"))
		{
			CloseGameUIDialog(d);
			sbo::ExecClientCmd("gameui_serverbrowser\n");
		}
		if (vgui2::VPANEL d = FindShownGameUIDialog("CreateMultiplayerGameDialog"))
		{
			CloseGameUIDialog(d);
			sbo::ExecClientCmd("gameui_createserver\n");
		}
		bool optionsOpening = false;
		if (vgui2::VPANEL d = FindShownGameUIDialog("OptionsDialog"))
		{
			const int page = GameUIOptionsPage(d, 0);
			CloseGameUIDialog(d);
			OpenWindowNextFrame(false, page >= 0 ? s_TabNames[page] : nullptr);
			optionsOpening = true;
		}
		const int oldPage = VGui2_OldAdvanced_CloseIfOpen();
		if (oldPage >= 0)
		{
			// the old dialog's tabs: General, Chat, Voice, Version (its Voice settings are under Options > Voice here)
			static const int s_OldToNew[] = { TAB_GENERAL, TAB_CHAT, TAB_GENERAL, TAB_VERSION };
			const int page = s_OldToNew[std::min(oldPage, 3)];
			if (optionsOpening)
				s_iDeferredAdvancedPage = page; // one window per frame: after the Options window
			else
				OpenWindowNextFrame(true, s_TabNames[page]);
		}
	}
	else
	{
		// back to GameUI's dialogs and SBO's old Advanced dialog (GameUI's dialogs open on their own first page)
		if (VGui2_ServerBrowser_CloseIfOpen())
			sbo::ExecClientCmd("gameui_serverbrowser\n");
		if (VGui2_CreateServer_CloseIfOpen())
			sbo::ExecClientCmd("gameui_createserver\n");
		if (CloseOptionsWindow(s_hOptions) >= 0)
			sbo::ExecClientCmd("gameui_options\n");
		const int page = CloseOptionsWindow(s_hAdvanced);
		if (page >= 0)
		{
			const int oldPage = page == TAB_CHAT ? 1 : (page == TAB_VERSION ? 3 : 0); // General, Chat, Voice, Version
			VGui2_OldAdvanced_OpenNextFrame(oldPage);
		}
	}
}

void VGui2_RegisterOptionsWindowCommand()
{
	static bool s_bDone = false;
	if (s_bDone)
		return;
	s_bDone = true;
	sbo::AddCommand("gameui_options", OptionsWindow_f);
}
