//========= SBO: Discord Rich Presence =========//
//
// Purpose: Minimal client for the Discord desktop app's local RPC (IPC) protocol, enough for Rich Presence:
//          connect, handshake, SET_ACTIVITY, PING/PONG, CLOSE. Everything runs on the caller's thread with
//          non-blocking I/O (call Update() every frame), so there is no worker thread to stop when the engine
//          unloads client.dll without a shutdown callback.
//
// Transport: the named pipe \\?\pipe\discord-ipc-<0..9> on Windows; on Linux/macOS the Unix socket
//            discord-ipc-<0..9> in $XDG_RUNTIME_DIR / $TMPDIR / /tmp (used by tests/discord_ipc_test).
//
// Protocol (same as discord-rpc, github.com/discord/discord-rpc, MIT): frames of
//   uint32 opcode (LE), uint32 length (LE), <length> bytes of UTF-8 JSON
//   opcodes: 0 HANDSHAKE {"v":1,"client_id":"..."}, 1 FRAME (commands/responses), 2 CLOSE, 3 PING, 4 PONG.
//
// No engine or SDK headers: the game glue is discord_rpc.cpp.
//==============================================//

#ifndef SBO_DISCORD_IPC_H
#define SBO_DISCORD_IPC_H

#include <stdint.h>
#include <string>

namespace discord
{

//! Escapes s for use inside a JSON string literal (quotes, backslash, control characters; UTF-8 passes through).
std::string JsonEscape(const char *s);
//! Value of the first "key" at any depth in json (string unescaped, or a number/literal as text), "" if missing.
//! Enough for Discord's small event payloads.
std::string JsonField(const std::string &json, const char *key);

class IpcClient
{
public:
	//! Messages (connect/disconnect/errors) go to this callback; may be null.
	typedef void (*LogFn)(const char *msg);
	//! Subscribed events (DISPATCH frames other than READY): evt name and the whole frame JSON.
	typedef void (*EventFn)(const char *evt, const std::string &json);

	IpcClient();
	~IpcClient();

	//! applicationId: the Discord application's ID (developer portal). events: subscribed to after each
	//! (re)connect (e.g. "ACTIVITY_JOIN"), delivered to onEvent; nullptr-terminated, may be null.
	void Init(const char *applicationId, LogFn log, const char *const *events = nullptr, EventFn onEvent = nullptr);

	//! Sends a command now (if connected): {"cmd":cmd,"args":argsJson,"nonce":...}. For replies to events, e.g.
	//! SEND_ACTIVITY_JOIN_INVITE. SET_ACTIVITY goes through SetActivity() (rate-limited) instead.
	bool SendCommand(const char *cmd, const std::string &argsJson);

	//! Call every frame. Reconnects (every few seconds while Discord isn't running), reads and answers incoming
	//! frames, and sends the latest activity once the handshake is done (at most one SET_ACTIVITY per
	//! MIN_UPDATE_INTERVAL, Discord's rate limit is 5 per 20 s). now: seconds, monotonic.
	void Update(double now);

	//! Sets the activity to show: the JSON object for "activity" (without pid), or "" to clear it. Sent on the
	//! next Update() if it differs from what Discord already has.
	void SetActivity(const std::string &activityJson);

	//! Clears the activity (if connected) and closes the connection. Init() state is kept; Update() reconnects.
	void Disconnect();

	bool IsReady() const { return m_state == State::Ready; }

	static constexpr double RECONNECT_INTERVAL = 10.0;
	static constexpr double MIN_UPDATE_INTERVAL = 5.0;

private:
	enum class State
	{
		Disconnected,
		Handshaking, //!< handshake sent, waiting for the READY dispatch
		Ready,
	};

	bool Open();
	void Close();
	bool WriteFrame(uint32_t opcode, const std::string &json);
	//! Reads what is available without blocking and handles complete frames. False if the connection broke.
	bool ReadFrames();
	void HandleFrame(uint32_t opcode, const std::string &json);
	void SendActivity();
	void Log(const char *fmt, ...);

	std::string m_appId;
	LogFn m_log = nullptr;
	const char *const *m_events = nullptr;
	EventFn m_onEvent = nullptr;
	State m_state = State::Disconnected;
	double m_nextConnect = 0;
	double m_lastSend = -1e9;
	std::string m_wanted;     //!< activity JSON the game wants shown ("" = none)
	std::string m_sent;       //!< activity JSON Discord has ("" = none)
	bool m_sentValid = false; //!< m_sent is known (false right after connecting)
	std::string m_readBuf;
	unsigned m_nonce = 0;

#ifdef _WIN32
	void *m_pipe;             //!< HANDLE
#else
	int m_sock = -1;
#endif
};

} // namespace discord

#endif // SBO_DISCORD_IPC_H
