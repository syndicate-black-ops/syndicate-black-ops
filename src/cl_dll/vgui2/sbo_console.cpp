//========= SBO 2.0: console colors (docs/CONSOLE_COLOR.md) =========//
//
// Purpose: Colored text in the game console: ^0..^9 / ^xRGB / ^#RRGGBB color codes in the client's console lines
//          are drawn as colors (hud_colortext 1), like the chat draws them.
//
// How (BugfixedHL-Rebased / Zombie Panic! console.cpp, HookConsoleColor): GameUI's console dialog keeps the color it
// prints Con_Printf text in (m_PrintColor) and the one for Con_DPrintf (m_DPrintColor) as two Color members. The
// dialog is reached through GameUI's GameConsole003 interface (tier2 g_pGameConsole: its CGameConsole holds
// m_bInitialized and the dialog pointer). The members' offsets differ between engine builds; each known pair is
// accepted only if both hold the colors the BaseUI scheme gives them (FgColor, BrightControlText). A colored print
// sets both members, prints, and puts the scheme colors back. No engine or GameUI code is changed or hooked.
//
// Fail safe: anything not found (no GameUI console, memory not writable, no offset pair matches, the colors changed
// behind our back) -> one console warning, then no colors for the rest of the session: callers print as before.
//
// SDK world: compiled with hl1_source_sdk include paths.
//=================================================================//

#define TIER2_GAMEUI_INTERNALS
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <tier2/tier2.h>
#include <IGameConsole.h>
#include <vgui/IScheme.h>
#include <Color.h>

#include "hud_chat.h"
#include "sbo_bridge.h"
#include "sbo_console_text.h"
#include "vgui2_api.h"

namespace
{

// Offsets of m_PrintColor and m_DPrintColor in GameUI's CGameConsoleDialog (BugfixedHL-Rebased console.cpp)
struct ColorOffset
{
	size_t printOffset;
	size_t dprintOffset;
};

constexpr ColorOffset COLOR_OFFSETS[] = {
#ifdef _WIN32
	ColorOffset { 0x128, 0x12C }, // before build 9884
	ColorOffset { 0x12C, 0x130 }, // 9884
	ColorOffset { 0x130, 0x134 }, // 9891 and later
#else
	ColorOffset { 0x124, 0x128 }, // before build 9884
	ColorOffset { 0x128, 0x12C }, // 9884
	ColorOffset { 0x12C, 0x130 }, // 9891 and later
#endif
};

// GameUI's CGameConsole: the interface, then its two members
class CGameConsolePrototype : public IGameConsole
{
public:
	bool m_bInitialized;
	void *m_pConsole;
};

// BaseUI scheme colors when the scheme cannot be read (GameUI's own defaults)
const Color DEFAULT_PRINT_COLOR(216, 222, 211, 255);
const Color DEFAULT_DPRINT_COLOR(196, 181, 80, 255);

enum class ConsoleColorState
{
	NotTried, //!< the console dialog did not exist yet: try again at the next colored print
	Ready,
	Failed,
};

ConsoleColorState s_State = ConsoleColorState::NotTried;
bool s_bWarned = false;
Color *s_pPrintColor = nullptr;
Color *s_pDPrintColor = nullptr;
Color s_PrintColor = DEFAULT_PRINT_COLOR;   // the console's own colors, put back after every colored print
Color s_DPrintColor = DEFAULT_DPRINT_COLOR;

void Warn(const char *reason)
{
	s_State = ConsoleColorState::Failed;
	s_pPrintColor = nullptr;
	s_pDPrintColor = nullptr;
	if (s_bWarned)
		return;
	s_bWarned = true;
	char msg[256];
	snprintf(msg, sizeof(msg), "Console colors are off: %s.\n", reason);
	sbo::DevPrint(msg);
}

// true if the n bytes at p are mapped readable (and writable when bWrite): checked before every access to GameUI's
// private memory, so a layout the offsets do not fit fails safe instead of crashing
bool IsMemoryAccessible(const void *p, size_t n, bool bWrite)
{
	if (!p || n == 0)
		return false;
	const uintptr_t start = (uintptr_t)p;
	const uintptr_t end = start + n;
	if (end < start)
		return false;
#ifdef _WIN32
	uintptr_t addr = start;
	while (addr < end)
	{
		MEMORY_BASIC_INFORMATION mbi;
		if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) != sizeof(mbi))
			return false;
		if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
			return false;
		const DWORD prot = mbi.Protect & 0xFF;
		const bool bReadable = prot == PAGE_READONLY || prot == PAGE_READWRITE || prot == PAGE_WRITECOPY ||
			prot == PAGE_EXECUTE_READ || prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY;
		const bool bWritable = prot == PAGE_READWRITE || prot == PAGE_WRITECOPY || prot == PAGE_EXECUTE_READWRITE ||
			prot == PAGE_EXECUTE_WRITECOPY;
		if (!bReadable || (bWrite && !bWritable))
			return false;
		const uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
		if (next <= addr)
			return false;
		addr = next;
	}
	return true;
#else
	// /proc/self/maps: "start-end perms ...", sorted by address; the range may span adjacent mappings
	FILE *f = fopen("/proc/self/maps", "r");
	if (!f)
		return false;
	uintptr_t covered = start;
	char line[512];
	bool bLineStart = true;
	while (covered < end && fgets(line, sizeof(line), f))
	{
		// a line longer than the buffer (long path) comes in parts: only a line's first part is parsed
		const bool bThisStart = bLineStart;
		bLineStart = strchr(line, '\n') != nullptr;
		if (!bThisStart)
			continue;
		unsigned long lo = 0, hi = 0;
		char perms[8] = {};
		if (sscanf(line, "%lx-%lx %7s", &lo, &hi, perms) != 3)
			continue;
		if (covered < lo || covered >= hi)
			continue;
		if (perms[0] != 'r' || (bWrite && perms[1] != 'w'))
			break;
		covered = hi;
	}
	fclose(f);
	return covered >= end;
#endif
}

void ReadSchemeColors()
{
	s_PrintColor = DEFAULT_PRINT_COLOR;
	s_DPrintColor = DEFAULT_DPRINT_COLOR;
	if (!g_pVGuiSchemeManager)
		return;
	vgui2::HScheme hScheme = g_pVGuiSchemeManager->GetScheme("BaseUI");
	vgui2::IScheme *pScheme = hScheme ? g_pVGuiSchemeManager->GetIScheme(hScheme) : nullptr;
	if (!pScheme)
		return;
	s_PrintColor = pScheme->GetColor("FgColor", DEFAULT_PRINT_COLOR);
	s_DPrintColor = pScheme->GetColor("BrightControlText", DEFAULT_DPRINT_COLOR);
}

// Finds the console dialog's two print colors (HookConsoleColor)
void FindConsoleColors()
{
	CGameConsolePrototype *pGameConsole = (CGameConsolePrototype *)g_pGameConsole;
	if (!pGameConsole || !IsMemoryAccessible(pGameConsole, sizeof(CGameConsolePrototype), false))
	{
		Warn("the GameUI console was not found");
		return;
	}

	void *pDialog = pGameConsole->m_pConsole;
	if (!pDialog)
		return; // not created yet: NotTried, the next colored print looks again

	ReadSchemeColors();

	for (const ColorOffset &offset : COLOR_OFFSETS)
	{
		Color *pPrint = (Color *)((char *)pDialog + offset.printOffset);
		Color *pDPrint = (Color *)((char *)pDialog + offset.dprintOffset);
		if (!IsMemoryAccessible(pPrint, sizeof(Color), true) || !IsMemoryAccessible(pDPrint, sizeof(Color), true))
			continue;
		if (*pPrint == s_PrintColor && *pDPrint == s_DPrintColor)
		{
			s_pPrintColor = pPrint;
			s_pDPrintColor = pDPrint;
			s_State = ConsoleColorState::Ready;
			return;
		}
	}

	Warn("this engine build's console was not recognized");
}

bool EnsureReady()
{
	if (s_State == ConsoleColorState::NotTried)
		FindConsoleColors();
	if (s_State != ConsoleColorState::Ready)
		return false;

	// The members hold the console's own colors between our prints; anything else means they are not what we think
	// (or the console changed them): stop for good
	if (*s_pPrintColor != s_PrintColor || *s_pDPrintColor != s_DPrintColor)
	{
		Warn("the console's colors changed");
		return false;
	}
	return true;
}

void SetColor(const Color &c)
{
	*s_pPrintColor = c;
	*s_pDPrintColor = c;
}

void ResetColor()
{
	*s_pPrintColor = s_PrintColor;
	*s_pDPrintColor = s_DPrintColor;
}

// Prints len bytes in pieces the engine's print buffer takes (whole UTF-8 characters)
void PrintRun(const char *s, int len)
{
	char buf[1024];
	while (len > 0)
	{
		const int n = SboConsole_PieceLength(s, len, (int)sizeof(buf) - 1);
		memcpy(buf, s, n);
		buf[n] = 0;
		sbo::ConsolePrint(buf);
		s += n;
		len -= n;
	}
}

} // namespace

// vgui2_api.h
void VGui2_Console_Init()
{
	if (s_State == ConsoleColorState::NotTried)
		FindConsoleColors();
}

// vgui2_api.h
bool VGui2_Console_PrintColorCodes(const char *text)
{
	if (!text || VGui2_Chat_GetColorCodeAction() != (int)ColorCodeAction::Handle)
		return false;
	if (!EnsureReady())
		return false;

	SboConsole_ForEachRun(
		text, [](const char *p) { return GetColorCodeLength(p); },
		[](const char *run, int len) { PrintRun(run, len); },
		[](const char *code, int len) {
			bool bReset = true;
			int rgb[3] = { 0, 0, 0 };
			VGui2_Chat_GetColorCode(code, &bReset, rgb);
			if (bReset)
				ResetColor();
			else
				SetColor(Color(rgb[0], rgb[1], rgb[2], 255));
		});
	ResetColor();
	return true;
}
