// sbo_a2s_rules.cpp: split-packet assembly (GoldSrc and Source headers), the rules parser, random / hostile packets.
// With "live <ip> <port>" it also queries that server once (run_test.sh does not).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <vector>

#include "sbo_a2s_rules.h"

static int s_iFailed = 0;
static void Check(bool ok, const char *what)
{
	if (!ok)
	{
		printf("FAIL: %s\n", what);
		s_iFailed++;
	}
}

static std::vector<uint8_t> RulesReply(int n)
{
	std::vector<uint8_t> r = { 0xFF, 0xFF, 0xFF, 0xFF, 'E', (uint8_t)(n & 255), (uint8_t)(n >> 8) };
	for (int i = 0; i < n; i++)
	{
		char k[32], v[32];
		snprintf(k, sizeof(k), "mp_rule_%d", i);
		snprintf(v, sizeof(v), "%d", i * 7);
		r.insert(r.end(), k, k + strlen(k) + 1);
		r.insert(r.end(), v, v + strlen(v) + 1);
	}
	return r;
}

static std::vector<std::vector<uint8_t>> Split(const std::vector<uint8_t> &whole, bool goldsrc, size_t chunk)
{
	std::vector<std::vector<uint8_t>> parts;
	const int total = (int)((whole.size() + chunk - 1) / chunk);
	for (int i = 0; i < total; i++)
	{
		std::vector<uint8_t> p = { 0xFE, 0xFF, 0xFF, 0xFF, 0x34, 0x12, 0, 0 };
		if (goldsrc)
			p.push_back((uint8_t)((i << 4) | total));
		else
		{
			p.push_back((uint8_t)total);
			p.push_back((uint8_t)i);
			p.push_back(0xE0);
			p.push_back(0x04);
		}
		const size_t from = (size_t)i * chunk, to = from + chunk < whole.size() ? from + chunk : whole.size();
		p.insert(p.end(), whole.begin() + from, whole.begin() + to);
		parts.push_back(p);
	}
	return parts;
}

int main(int argc, char **argv)
{
	const std::vector<uint8_t> whole = RulesReply(90);
	sbo_a2s::Rules rules;
	Check(sbo_a2s::ParseRules(whole.data(), whole.size(), rules) && rules.size() == 90 && rules[89].first == "mp_rule_89"
	          && rules[89].second == "623", "whole reply");

	for (int gs = 0; gs < 2; gs++)
	{
		std::vector<std::vector<uint8_t>> parts = Split(whole, gs == 1, 1200);
		// out of order: last part first
		sbo_a2s::Assembler a;
		std::vector<uint8_t> reply;
		bool done = false;
		for (size_t i = parts.size(); i-- > 0;)
			done = a.Feed(parts[i].data(), parts[i].size(), reply);
		Check(done && reply == whole, gs ? "GoldSrc split" : "Source split");
	}

	// cut reply: what came is kept
	Check(sbo_a2s::ParseRules(whole.data(), 200, rules) && !rules.empty() && rules.size() < 90, "cut reply");
	Check(!sbo_a2s::ParseRules(whole.data(), 4, rules), "too short");

	// random and damaged packets
	srand(777);
	for (int n = 0; n < 100000; n++)
	{
		std::vector<std::vector<uint8_t>> parts = Split(whole, rand() & 1, 200 + rand() % 1200);
		sbo_a2s::Assembler a;
		std::vector<uint8_t> reply;
		for (std::vector<uint8_t> &p : parts)
		{
			const int edits = rand() % 4;
			for (int e = 0; e < edits; e++)
				p[(size_t)rand() % p.size()] = (uint8_t)rand();
			if (rand() % 5 == 0)
				p.resize((size_t)rand() % (p.size() + 1));
			if (a.Feed(p.data(), p.size(), reply))
				sbo_a2s::ParseRules(reply.data(), reply.size(), rules);
		}
		std::vector<uint8_t> junk((size_t)rand() % 1500);
		for (uint8_t &b : junk)
			b = (uint8_t)rand();
		sbo_a2s::ParseRules(junk.data(), junk.size(), rules);
		a.Feed(junk.data(), junk.size(), reply);
	}

	if (argc == 4 && !strcmp(argv[1], "live"))
	{
		unsigned a, b, c, d;
		if (sscanf(argv[2], "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
		{
			sbo_a2s::Query q;
			q.Start((a << 24) | (b << 16) | (c << 8) | d, (uint16_t)atoi(argv[3]), 0.0);
			double t = 0.0;
			struct timespec ts = { 0, 20000000 };
			while (!q.Poll(t))
			{
				nanosleep(&ts, nullptr);
				t += 0.02;
			}
			printf("live: %s, %d rules\n", q.Succeeded() ? "ok" : "failed", (int)q.GetRules().size());
			for (const auto &r : q.GetRules())
				if (r.first == "mp_friendlyfire" || r.first == "mp_timeleft")
					printf("  %s = %s\n", r.first.c_str(), r.second.c_str());
		}
	}

	if (s_iFailed)
	{
		printf("%d check(s) failed\n", s_iFailed);
		return 1;
	}
	printf("PASS\n");
	return 0;
}
