//========= SBO VGUI2: trigger_objective HUD list =========//
//
// Purpose: SBO side of the objective list (hud_objectives.cpp): a titles.txt message's text for an objective named
//          "#NAME". Its own file, so the list doesn't touch the other bridge files.
//
// SBO world: compiled with SBO's client include paths (SDK 2.3) + sbo_compat.h. No Source SDK headers.
//=================================================================//

#include "hud.h"
#include "cl_util.h"

namespace sbo
{
// titles.txt's message NAME (the engine's list, as env_message / game_text names use it): its text with line breaks
// as spaces. False: no such message.
bool ObjectiveTitlesText(const char *name, char *out, int outSize)
{
	if (!name || !name[0] || !out || outSize <= 0)
		return false;
	client_textmessage_t *pMessage = gEngfuncs.pfnTextMessageGet(name);
	if (!pMessage || !pMessage->pMessage)
		return false;

	int n = 0;
	for (const char *p = pMessage->pMessage; *p && n < outSize - 1; p++)
	{
		char c = *p;
		if (c == '\r')
			continue;
		if (c == '\n')
			c = ' ';
		out[n++] = c;
	}
	// no trailing space from a final line break
	while (n > 0 && out[n - 1] == ' ')
		n--;
	out[n] = '\0';
	return true;
}
} // namespace sbo
