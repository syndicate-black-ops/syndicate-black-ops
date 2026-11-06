//========= SBO: Discord Rich Presence =========//
//
// Purpose: Discord RPC (IPC) client, see discord_ipc.h.
//==============================================//

#include "discord_ipc.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace discord
{

enum Opcode : uint32_t
{
	OP_HANDSHAKE = 0,
	OP_FRAME = 1,
	OP_CLOSE = 2,
	OP_PING = 3,
	OP_PONG = 4,
};

// Frames larger than this are a protocol error (Discord's own limit is 64 KiB).
static const uint32_t MAX_FRAME = 64 * 1024;

std::string JsonEscape(const char *s)
{
	std::string out;
	if (!s)
		return out;
	for (; *s; s++)
	{
		unsigned char c = (unsigned char)*s;
		switch (c)
		{
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20)
			{
				char buf[8];
				snprintf(buf, sizeof(buf), "\\u%04x", c);
				out += buf;
			}
			else
				out += (char)c;
		}
	}
	return out;
}

//-----------------------------------------------------------------------------
// Tiny JSON field lookup for the few response fields we need ("cmd", "evt", "code", "message"): finds
// "key":"value" (string) or "key":number at any depth. Good enough for Discord's flat responses.
//-----------------------------------------------------------------------------
std::string JsonField(const std::string &json, const char *key)
{
	std::string pat = std::string("\"") + key + "\"";
	size_t p = json.find(pat);
	if (p == std::string::npos)
		return "";
	p = json.find(':', p + pat.size());
	if (p == std::string::npos)
		return "";
	p++;
	while (p < json.size() && (json[p] == ' ' || json[p] == '\t'))
		p++;
	if (p >= json.size())
		return "";
	if (json[p] == '"')
	{
		std::string v;
		for (p++; p < json.size() && json[p] != '"'; p++)
		{
			if (json[p] == '\\' && p + 1 < json.size())
				p++;
			v += json[p];
		}
		return v;
	}
	size_t e = json.find_first_of(",}]", p);
	return json.substr(p, e == std::string::npos ? std::string::npos : e - p);
}

IpcClient::IpcClient()
{
#ifdef _WIN32
	m_pipe = INVALID_HANDLE_VALUE;
#endif
}

IpcClient::~IpcClient()
{
	Close();
}

void IpcClient::Init(const char *applicationId, LogFn log, const char *const *events, EventFn onEvent)
{
	m_appId = applicationId ? applicationId : "";
	m_log = log;
	m_events = events;
	m_onEvent = onEvent;
}

bool IpcClient::SendCommand(const char *cmd, const std::string &argsJson)
{
	if (m_state != State::Ready)
		return false;
	char head[96];
	snprintf(head, sizeof(head), "{\"cmd\":\"%s\",\"nonce\":\"%u\",\"args\":", cmd, ++m_nonce);
	return WriteFrame(OP_FRAME, head + argsJson + "}");
}

void IpcClient::Log(const char *fmt, ...)
{
	if (!m_log)
		return;
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	m_log(buf);
}

void IpcClient::SetActivity(const std::string &activityJson)
{
	m_wanted = activityJson;
}

void IpcClient::Update(double now)
{
	if (m_appId.empty())
		return;

	if (m_state == State::Disconnected)
	{
		if (now < m_nextConnect)
			return;
		m_nextConnect = now + RECONNECT_INTERVAL;
		if (!Open())
			return;
		m_readBuf.clear();
		m_sentValid = false;
		if (!WriteFrame(OP_HANDSHAKE, "{\"v\":1,\"client_id\":\"" + JsonEscape(m_appId.c_str()) + "\"}"))
		{
			Close();
			return;
		}
		m_state = State::Handshaking;
	}

	if (!ReadFrames())
	{
		Log("Discord: connection lost.\n");
		Close();
		m_nextConnect = now + RECONNECT_INTERVAL;
		return;
	}

	if (m_state == State::Ready && (!m_sentValid || m_sent != m_wanted) && now - m_lastSend >= MIN_UPDATE_INTERVAL)
	{
		m_lastSend = now;
		SendActivity();
	}
}

void IpcClient::SendActivity()
{
#ifdef _WIN32
	unsigned long pid = GetCurrentProcessId();
#else
	unsigned long pid = (unsigned long)getpid();
#endif
	char head[96];
	snprintf(head, sizeof(head), "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"%u\",\"args\":{\"pid\":%lu,\"activity\":", ++m_nonce, pid);
	std::string json = head;
	json += m_wanted.empty() ? "null" : m_wanted;
	json += "}}";
	if (WriteFrame(OP_FRAME, json))
	{
		m_sent = m_wanted;
		m_sentValid = true;
	}
}

void IpcClient::Disconnect()
{
	if (m_state == State::Ready && m_sentValid && !m_sent.empty())
	{
		m_wanted.clear();
		SendActivity();
	}
	Close();
	m_wanted.clear();
	m_nextConnect = 0;
}

void IpcClient::HandleFrame(uint32_t opcode, const std::string &json)
{
	switch (opcode)
	{
	case OP_FRAME:
	{
		std::string cmd = JsonField(json, "cmd");
		std::string evt = JsonField(json, "evt");
		if (cmd == "DISPATCH" && evt == "READY")
		{
			m_state = State::Ready;
			for (const char *const *e = m_events; e && *e; e++)
			{
				char sub[160];
				snprintf(sub, sizeof(sub), "{\"cmd\":\"SUBSCRIBE\",\"evt\":\"%s\",\"nonce\":\"%u\",\"args\":{}}", *e, ++m_nonce);
				WriteFrame(OP_FRAME, sub);
			}
		}
		else if (evt == "ERROR")
			Log("Discord: %s error %s: %s\n", cmd.c_str(), JsonField(json, "code").c_str(), JsonField(json, "message").c_str());
		else if (cmd == "DISPATCH" && !evt.empty() && m_onEvent)
			m_onEvent(evt.c_str(), json);
		break;
	}
	case OP_PING:
		WriteFrame(OP_PONG, json);
		break;
	case OP_CLOSE:
		Log("Discord: closed the connection (%s: %s).\n", JsonField(json, "code").c_str(), JsonField(json, "message").c_str());
		Close();
		break;
	default:
		break;
	}
}

bool IpcClient::ReadFrames()
{
	char buf[4096];
	for (;;)
	{
		long n;
#ifdef _WIN32
		DWORD avail = 0;
		if (!PeekNamedPipe((HANDLE)m_pipe, nullptr, 0, nullptr, &avail, nullptr))
			return false;
		if (avail == 0)
			break;
		DWORD got = 0;
		if (!ReadFile((HANDLE)m_pipe, buf, avail < sizeof(buf) ? avail : (DWORD)sizeof(buf), &got, nullptr))
			return false;
		n = (long)got;
#else
		n = (long)recv(m_sock, buf, sizeof(buf), 0);
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			break;
		if (n <= 0)
			return false;
#endif
		m_readBuf.append(buf, (size_t)n);
	}

	while (m_readBuf.size() >= 8)
	{
		const unsigned char *h = (const unsigned char *)m_readBuf.data();
		uint32_t opcode = h[0] | (h[1] << 8) | (h[2] << 16) | ((uint32_t)h[3] << 24);
		uint32_t len = h[4] | (h[5] << 8) | (h[6] << 16) | ((uint32_t)h[7] << 24);
		if (len > MAX_FRAME)
			return false;
		if (m_readBuf.size() < 8 + (size_t)len)
			break;
		std::string json = m_readBuf.substr(8, len);
		m_readBuf.erase(0, 8 + (size_t)len);
		HandleFrame(opcode, json);
		if (m_state == State::Disconnected) // OP_CLOSE
			return true;
	}
	return true;
}

bool IpcClient::WriteFrame(uint32_t opcode, const std::string &json)
{
	if (json.size() > MAX_FRAME)
		return false;
	std::string frame(8, '\0');
	uint32_t len = (uint32_t)json.size();
	for (int i = 0; i < 4; i++)
	{
		frame[i] = (char)((opcode >> (8 * i)) & 0xff);
		frame[4 + i] = (char)((len >> (8 * i)) & 0xff);
	}
	frame += json;

#ifdef _WIN32
	DWORD written = 0;
	if (!WriteFile((HANDLE)m_pipe, frame.data(), (DWORD)frame.size(), &written, nullptr) || written != frame.size())
	{
		Close();
		return false;
	}
#else
	size_t off = 0;
	while (off < frame.size())
	{
		ssize_t n = send(m_sock, frame.data() + off, frame.size() - off, MSG_NOSIGNAL);
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			continue; // tiny frames; the socket buffer drains immediately
		if (n <= 0)
		{
			Close();
			return false;
		}
		off += (size_t)n;
	}
#endif
	return true;
}

bool IpcClient::Open()
{
	Close();
#ifdef _WIN32
	for (int i = 0; i < 10; i++)
	{
		wchar_t name[64];
		swprintf(name, 64, L"\\\\?\\pipe\\discord-ipc-%d", i);
		HANDLE h = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		if (h != INVALID_HANDLE_VALUE)
		{
			m_pipe = h;
			return true;
		}
	}
	return false;
#else
	const char *dir = getenv("XDG_RUNTIME_DIR");
	if (!dir || !*dir)
		dir = getenv("TMPDIR");
	if (!dir || !*dir)
		dir = "/tmp";
	for (int i = 0; i < 10; i++)
	{
		sockaddr_un addr{};
		addr.sun_family = AF_UNIX;
		snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/discord-ipc-%d", dir, i);
		int s = socket(AF_UNIX, SOCK_STREAM, 0);
		if (s < 0)
			return false;
		if (connect(s, (sockaddr *)&addr, sizeof(addr)) == 0)
		{
			fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
			m_sock = s;
			return true;
		}
		close(s);
	}
	return false;
#endif
}

void IpcClient::Close()
{
#ifdef _WIN32
	if (m_pipe != INVALID_HANDLE_VALUE)
		CloseHandle((HANDLE)m_pipe);
	m_pipe = INVALID_HANDLE_VALUE;
#else
	if (m_sock >= 0)
		close(m_sock);
	m_sock = -1;
#endif
	m_state = State::Disconnected;
	m_readBuf.clear();
	m_sentValid = false;
}

} // namespace discord
