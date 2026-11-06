// Windows twin of fake_discord.py for the named-pipe transport: serves \\.\pipe\discord-ipc-0, answers the
// handshake with READY, sends a PING after the first activity, and writes one line per received frame to the log
// file ("<opcode> <json>"). Two sessions (the driver disconnects once), then exits. Checked by run_test_wine.sh.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string>

static bool ReadAll(HANDLE h, void *buf, DWORD n)
{
	char *p = (char *)buf;
	while (n)
	{
		DWORD got = 0;
		if (!ReadFile(h, p, n, &got, nullptr) || !got)
			return false;
		p += got;
		n -= got;
	}
	return true;
}

static void Send(HANDLE h, uint32_t op, const std::string &json)
{
	uint32_t hdr[2] = {op, (uint32_t)json.size()};
	DWORD w;
	WriteFile(h, hdr, 8, &w, nullptr);
	WriteFile(h, json.data(), (DWORD)json.size(), &w, nullptr);
}

int main(int argc, char **argv)
{
	FILE *log = fopen(argc > 1 ? argv[1] : "pipe.log", "wb");
	for (int session = 0; session < 2; session++)
	{
		HANDLE h = CreateNamedPipeW(L"\\\\.\\pipe\\discord-ipc-0", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
		                            1, 65536, 65536, 0, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return 2;
		if (!ConnectNamedPipe(h, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED)
			return 3;
		bool pinged = false;
		for (;;)
		{
			uint32_t hdr[2];
			if (!ReadAll(h, hdr, 8))
				break;
			std::string json(hdr[1], '\0');
			if (hdr[1] && !ReadAll(h, &json[0], hdr[1]))
				break;
			fprintf(log, "%d %u %s\n", session, hdr[0], json.c_str());
			fflush(log);
			if (hdr[0] == 0)
				Send(h, 1, "{\"cmd\":\"DISPATCH\",\"evt\":\"READY\",\"data\":{\"v\":1}}");
			else if (hdr[0] == 1 && json.find("\"SUBSCRIBE\"") != std::string::npos)
			{
				if (session == 0 && json.find("ACTIVITY_JOIN_REQUEST") != std::string::npos)
				{
					Send(h, 1, "{\"cmd\":\"DISPATCH\",\"evt\":\"ACTIVITY_JOIN\",\"data\":{\"secret\":\"203.0.113.4:27015\"}}");
					Send(h, 1, "{\"cmd\":\"DISPATCH\",\"evt\":\"ACTIVITY_JOIN_REQUEST\",\"data\":{\"user\":{\"id\":\"987654321\",\"username\":\"friend\"}}}");
				}
			}
			else if (hdr[0] == 1 && json.find("SET_ACTIVITY") != std::string::npos && !pinged)
			{
				Send(h, 3, "{\"ping\":42}");
				pinged = true;
			}
		}
		fprintf(log, "%d closed\n", session);
		fflush(log);
		DisconnectNamedPipe(h);
		CloseHandle(h);
	}
	fclose(log);
	return 0;
}
