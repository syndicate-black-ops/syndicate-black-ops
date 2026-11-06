//========= SBO VGUI2: server browser addresses =========//
//
// Purpose: "host[:port]" text (the server browser's Add server dialog) to an IPv4 address and port. The host is a
//          dotted IPv4 address or a host name (resolved with the system resolver, as Aura's / Valve's CDialogAddServer
//          does with gethostbyname; on Linux through <netdb.h> instead of Windows' winsock.h). No vgui or SDK headers,
//          so the parser has its own sanitizer test (tests/server_browser/run_test.sh).
//=================================================================//

#ifndef SBO_VGUI2_SBO_SERVER_ADDRESS_H
#define SBO_VGUI2_SBO_SERVER_ADDRESS_H

#include <stdint.h>

namespace sbo_address
{
constexpr uint16_t DEFAULT_PORT = 27015;

//! Splits "host[:port]" (spaces around it ignored) into host and port. host gets at most hostSize - 1 characters
//! (a longer host fails). No port: DEFAULT_PORT. False for an empty host, a port that is not 1-65535, or text after
//! the port.
bool Split(const char *text, char *host, int hostSize, uint16_t &port);

//! A dotted IPv4 address ("a.b.c.d", each 0-255) in host byte order. False for anything else.
bool ParseIPv4(const char *host, uint32_t &ip);

//! Split + ParseIPv4, or a name lookup when the host is not an IPv4 address. ip in host byte order (what
//! ISteamMatchmaking / servernetadr_t take). False if the text does not parse or the name does not resolve.
bool Resolve(const char *text, uint32_t &ip, uint16_t &port);
} // namespace sbo_address

#endif // SBO_VGUI2_SBO_SERVER_ADDRESS_H
