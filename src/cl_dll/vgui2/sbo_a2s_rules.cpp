//========= SBO VGUI2: server browser A2S_RULES query =========//
//
// Purpose: see sbo_a2s_rules.h.
//=================================================================//

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h> // socket API (wsock32, which client.dll links already)
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <string.h>

#include "sbo_a2s_rules.h"

namespace
{
constexpr size_t MAX_PACKET = 1400;
constexpr int MAX_PARTS = 16;
constexpr size_t MAX_RULES = 512;
constexpr size_t MAX_STRING = 255;
constexpr double TIMEOUT_S = 3.0;

int32_t ReadInt32(const uint8_t *p)
{
	return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

void CloseSocket(intptr_t s)
{
#ifdef _WIN32
	closesocket((SOCKET)s);
#else
	close((int)s);
#endif
}
} // namespace

bool sbo_a2s::Assembler::Feed(const uint8_t *data, size_t size, std::vector<uint8_t> &reply)
{
	if (!data || size < 5 || size > MAX_PACKET + 64)
		return false;
	const int32_t header = ReadInt32(data);
	if (header == -1)
	{
		reply.assign(data, data + size);
		return true;
	}
	if (header != -2 || size < 9)
		return false;

	// GoldSrc: -2, int id, byte (number << 4 | total), payload. Source: -2, int id, byte total, byte number,
	// short size, payload. Told apart by the GoldSrc byte making sense with the first part's header.
	const int32_t id = ReadInt32(data + 4);
	int total, number;
	size_t offset;
	const int gsTotal = data[8] & 0x0F, gsNumber = data[8] >> 4;
	if (gsTotal >= 1 && gsNumber < gsTotal && (gsNumber != 0 || (size >= 13 && ReadInt32(data + 9) == -1)))
	{
		total = gsTotal;
		number = gsNumber;
		offset = 9;
	}
	else
	{
		if (size < 12)
			return false;
		total = data[8];
		number = data[9];
		offset = 12;
	}
	if (total < 1 || total > MAX_PARTS || number >= total || offset >= size)
		return false;

	if (m_iTotal != total || m_iId != id)
	{
		m_iId = id;
		m_iTotal = total;
		m_Parts.assign((size_t)total, std::vector<uint8_t>());
	}
	m_Parts[(size_t)number].assign(data + offset, data + size);
	for (const std::vector<uint8_t> &part : m_Parts)
	{
		if (part.empty())
			return false;
	}
	reply.clear();
	for (const std::vector<uint8_t> &part : m_Parts)
		reply.insert(reply.end(), part.begin(), part.end());
	m_iTotal = 0;
	return reply.size() >= 5 && ReadInt32(reply.data()) == -1;
}

bool sbo_a2s::ParseRules(const uint8_t *data, size_t size, Rules &rules)
{
	rules.clear();
	if (!data || size < 7 || ReadInt32(data) != -1 || data[4] != 'E')
		return false;
	const size_t count = (size_t)data[5] | ((size_t)data[6] << 8);
	size_t pos = 7;
	auto readString = [&](std::string &out) {
		out.clear();
		while (pos < size && data[pos])
		{
			if (out.size() < MAX_STRING)
				out += (char)data[pos];
			pos++;
		}
		if (pos >= size)
			return false;
		pos++; // the terminator
		return true;
	};
	for (size_t i = 0; i < count && rules.size() < MAX_RULES; i++)
	{
		std::string name, value;
		if (!readString(name) || !readString(value))
			break; // a cut reply: keep what came
		rules.emplace_back(name, value);
	}
	return !rules.empty() || count == 0;
}

sbo_a2s::Query::~Query()
{
	Cancel();
}

void sbo_a2s::Query::Cancel()
{
	if (m_iSocket != -1)
		CloseSocket(m_iSocket);
	m_iSocket = -1;
}

void sbo_a2s::Query::Send(const uint8_t *data, size_t size)
{
	sockaddr_in to;
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = htonl(m_uIp);
	to.sin_port = htons(m_uPort);
#ifdef _WIN32
	sendto((SOCKET)m_iSocket, (const char *)data, (int)size, 0, (const sockaddr *)&to, sizeof(to));
#else
	sendto((int)m_iSocket, data, size, 0, (const sockaddr *)&to, sizeof(to));
#endif
}

bool sbo_a2s::Query::Start(uint32_t ip, uint16_t port, double now)
{
	Cancel();
	m_bOk = false;
	m_Rules.clear();
	m_Assembler = Assembler();
	m_uIp = ip;
	m_uPort = port;
#ifdef _WIN32
	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET)
		return false;
	u_long nonBlocking = 1;
	ioctlsocket(s, FIONBIO, &nonBlocking);
	m_iSocket = (intptr_t)s;
#else
	int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		return false;
	fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
	m_iSocket = s;
#endif
	m_flDeadline = now + TIMEOUT_S;
	// A2S_RULES with the "no challenge" value: the server answers with a challenge (or the rules)
	const uint8_t request[] = { 0xFF, 0xFF, 0xFF, 0xFF, 'V', 0xFF, 0xFF, 0xFF, 0xFF };
	Send(request, sizeof(request));
	return true;
}

bool sbo_a2s::Query::Poll(double now)
{
	if (m_iSocket == -1)
		return true;
	uint8_t buf[MAX_PACKET + 64];
	for (int i = 0; i < 32; i++)
	{
		sockaddr_in from;
		socklen_t fromLen = sizeof(from);
#ifdef _WIN32
		const int n = recvfrom((SOCKET)m_iSocket, (char *)buf, (int)sizeof(buf), 0, (sockaddr *)&from, &fromLen);
#else
		const int n = (int)recvfrom((int)m_iSocket, buf, sizeof(buf), 0, (sockaddr *)&from, &fromLen);
#endif
		if (n <= 0)
			break;
		if (from.sin_addr.s_addr != htonl(m_uIp) || from.sin_port != htons(m_uPort))
			continue; // not from that server
		if (n >= 9 && ReadInt32(buf) == -1 && buf[4] == 'A')
		{
			// the challenge: ask again with it
			uint8_t request[9] = { 0xFF, 0xFF, 0xFF, 0xFF, 'V' };
			memcpy(request + 5, buf + 5, 4);
			Send(request, sizeof(request));
			continue;
		}
		std::vector<uint8_t> reply;
		if (m_Assembler.Feed(buf, (size_t)n, reply) && ParseRules(reply.data(), reply.size(), m_Rules))
		{
			m_bOk = true;
			Cancel();
			return true;
		}
	}
	if (now >= m_flDeadline)
	{
		Cancel();
		return true;
	}
	return false;
}
