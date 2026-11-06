//========= SBO VGUI2: server browser addresses =========//
//
// Purpose: see sbo_server_address.h.
//=================================================================//

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h> // gethostbyname (wsock32, which client.dll links already)
#else
#include <netdb.h>      // gethostbyname
#include <arpa/inet.h>  // ntohl
#include <netinet/in.h>
#endif

#include <string.h>

#include "sbo_server_address.h"

namespace
{
bool IsSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
} // namespace

bool sbo_address::Split(const char *text, char *host, int hostSize, uint16_t &port)
{
	if (!text || !host || hostSize <= 1)
		return false;
	host[0] = 0;
	port = DEFAULT_PORT;

	while (IsSpace(*text))
		text++;

	int n = 0;
	while (*text && *text != ':' && !IsSpace(*text))
	{
		if (n >= hostSize - 1)
			return false;
		host[n++] = *text++;
	}
	host[n] = 0;
	if (n == 0)
		return false;

	if (*text == ':')
	{
		text++;
		unsigned value = 0;
		int digits = 0;
		while (*text >= '0' && *text <= '9')
		{
			value = value * 10 + (unsigned)(*text - '0');
			if (++digits > 5 || value > 65535)
				return false;
			text++;
		}
		if (digits == 0 || value == 0)
			return false;
		port = (uint16_t)value;
	}

	while (IsSpace(*text))
		text++;
	return *text == 0;
}

bool sbo_address::ParseIPv4(const char *host, uint32_t &ip)
{
	if (!host)
		return false;
	uint32_t result = 0;
	for (int part = 0; part < 4; part++)
	{
		unsigned value = 0;
		int digits = 0;
		while (*host >= '0' && *host <= '9')
		{
			value = value * 10 + (unsigned)(*host - '0');
			if (++digits > 3 || value > 255)
				return false;
			host++;
		}
		if (digits == 0)
			return false;
		result = (result << 8) | value;
		if (part < 3)
		{
			if (*host != '.')
				return false;
			host++;
		}
	}
	if (*host)
		return false;
	ip = result;
	return true;
}

bool sbo_address::Resolve(const char *text, uint32_t &ip, uint16_t &port)
{
	char host[256];
	if (!Split(text, host, sizeof(host), port))
		return false;
	if (ParseIPv4(host, ip))
		return ip != 0;

	// A host name: letters, digits, '-' and '.' only (no lookup of anything else)
	for (const char *c = host; *c; c++)
	{
		const bool ok = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '-'
		                || *c == '.';
		if (!ok)
			return false;
	}

	const hostent *h = gethostbyname(host);
	if (!h || h->h_addrtype != AF_INET || h->h_length != 4 || !h->h_addr_list || !h->h_addr_list[0])
		return false;
	uint32_t netOrder;
	memcpy(&netOrder, h->h_addr_list[0], 4);
	ip = ntohl(netOrder);
	return ip != 0;
}
