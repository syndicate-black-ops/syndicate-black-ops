// Test driver for src/cl_dll/discord/discord_ipc.cpp (POSIX transport), run by run_test.sh against fake_discord.py.
// Scenario (time is simulated, so the rate limit is exercised without sleeping in real time):
//   t=0   activity A (with characters that need JSON escaping)
//   t=1   activity B  -> must wait for the 5 s rate limit, then be sent
//   t=12  Disconnect() -> clears the activity (activity null) and closes
//   t=13  reconnect, activity C
#include "discord_ipc.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static void usleep(unsigned us) { Sleep(us / 1000); }
#else
#include <unistd.h>
#endif

static void Log(const char *m) { fputs(m, stderr); }

static discord::IpcClient c;
static int s_joins, s_requests;
static void OnEvent(const char *evt, const std::string &json)
{
	if (!strcmp(evt, "ACTIVITY_JOIN") && discord::JsonField(json, "secret") == "203.0.113.4:27015")
		s_joins++;
	else if (!strcmp(evt, "ACTIVITY_JOIN_REQUEST"))
	{
		s_requests++;
		c.SendCommand("SEND_ACTIVITY_JOIN_INVITE", "{\"user_id\":\"" + discord::JsonField(json, "id") + "\"}");
	}
}

int main()
{
	static const char *const events[] = {"ACTIVITY_JOIN", "ACTIVITY_JOIN_REQUEST", nullptr};
	c.Init("123456789012345678", Log, events, OnEvent);
	double t = 0;
	auto run = [&](double until) {
		for (; t < until; t += 0.05)
		{
			c.Update(t);
			usleep(2000);
		}
	};
	c.SetActivity("{\"details\":\"" + discord::JsonEscape("Srv \"quoted\" \\ tab\t\x01 \xc3\xa9") + "\"}");
	run(1);
	if (!c.IsReady()) { fprintf(stderr, "not ready after handshake\n"); return 1; }
	c.SetActivity("{\"details\":\"B\"}");
	run(12);
	c.Disconnect();
	if (c.IsReady()) { fprintf(stderr, "still ready after Disconnect\n"); return 1; }
	c.SetActivity("{\"details\":\"C\"}");
	run(35); // > RECONNECT_INTERVAL: the first reconnect attempt may come before the server is listening again
	if (s_joins != 1 || s_requests != 1) { fprintf(stderr, "events: joins %d requests %d\n", s_joins, s_requests); return 1; }
	return c.IsReady() ? 0 : 1;
}
