//========= SBO VGUI2: server browser A2S_RULES query =========//
//
// Purpose: the server browser asks the selected server for its rules (mp_friendlyfire, mp_timeleft). Steam's
//          ISteamMatchmakingServers::ServerRules gets no answer from SBO's servers: their rule list (about 90 cvars,
//          2.6 KB) comes in several packets in GoldSrc's split format (header -2, id, one byte packet number / count),
//          which Steam's query does not put together. This is the plain A2S_RULES exchange over UDP (challenge, then
//          the rules), polled from the panel, with both GoldSrc and Source split headers.
//          No SDK headers; the reply parser has a sanitizer test (tests/server_browser/run_test.sh).
//=================================================================//

#ifndef SBO_VGUI2_SBO_A2S_RULES_H
#define SBO_VGUI2_SBO_A2S_RULES_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace sbo_a2s
{
using Rules = std::vector<std::pair<std::string, std::string>>;

//! Puts split packets together: feed every datagram; true once a whole reply is in `reply` (starting with the
//! 0xFFFFFFFF header). Bounded: at most 16 packets of at most 1400 bytes each.
class Assembler
{
public:
	bool Feed(const uint8_t *data, size_t size, std::vector<uint8_t> &reply);

private:
	int32_t m_iId = 0;
	int m_iTotal = 0;
	std::vector<std::vector<uint8_t>> m_Parts;
};

//! A whole A2S_RULES reply ("\xFF\xFF\xFF\xFFE", count, name/value strings). False if it is not one; at most 512
//! rules, names and values cut at 255 characters.
bool ParseRules(const uint8_t *data, size_t size, Rules &rules);

//! One query: Start, then Poll every frame until it returns true (done or failed).
class Query
{
public:
	~Query();
	//! ip / port in host byte order. False if no socket could be made.
	bool Start(uint32_t ip, uint16_t port, double now);
	//! True when finished; then Succeeded() / GetRules().
	bool Poll(double now);
	void Cancel();
	bool IsRunning() const { return m_iSocket != -1; }
	bool Succeeded() const { return m_bOk; }
	const Rules &GetRules() const { return m_Rules; }

private:
	void Send(const uint8_t *data, size_t size);
	intptr_t m_iSocket = -1;
	uint32_t m_uIp = 0;
	uint16_t m_uPort = 0;
	double m_flDeadline = 0.0;
	bool m_bOk = false;
	Assembler m_Assembler;
	Rules m_Rules;
};
} // namespace sbo_a2s

#endif // SBO_VGUI2_SBO_A2S_RULES_H
