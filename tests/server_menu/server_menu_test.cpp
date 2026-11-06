// Server menu protocol test (tests/server_menu/run_test.sh): the client's parser and store
// (src/cl_dll/vgui2/sbo_server_menu_proto.cpp) alone, with AddressSanitizer and UndefinedBehaviorSanitizer. Round trips
// of the records the API plugin writes (server_menu_api.sma), then hostile input: every prefix of a valid message,
// random bytes, indices and counts past the caps, strings without their end, the text limit, bad paths. With the
// Pawn include's path as argument it also checks that the include's numbers equal server_menu_shared.h's.
// Exit 1 on a failed check (a sanitizer error aborts).

#include "sbo_server_menu_proto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

using namespace servermenu;

static int s_iFailures = 0;
static int s_iChecks = 0;

#define CHECK(cond) do { s_iChecks++; if (!(cond)) { s_iFailures++; \
	fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static unsigned s_uSeed = 12345;
static unsigned Rand()
{
	s_uSeed = s_uSeed * 1103515245u + 12345u;
	return (s_uSeed >> 8) & 0xFFFFFF;
}

//---------------------------------------------------------
// The API plugin's writer, again without the engine
//---------------------------------------------------------
struct W
{
	std::vector<unsigned char> b;
	size_t lenAt = (size_t)-1; //!< the current record's length byte (kept up to date as its fields are written)
	void Put(int v)
	{
		b.push_back((unsigned char)v);
		if (lenAt != (size_t)-1)
			b[lenAt] = (unsigned char)(b.size() - lenAt - 1);
	}
	//! A new record: its op and its length byte
	W &Op(int op) { lenAt = (size_t)-1; b.push_back((unsigned char)op); b.push_back(0); lenAt = b.size() - 1; return *this; }
	W &Byte(int v) { Put(v); return *this; }
	W &Short(int v) { Put(v & 0xFF); Put((v >> 8) & 0xFF); return *this; }
	W &Long64(unsigned long long v) { for (int i = 0; i < 8; i++) Put((int)((v >> (8 * i)) & 0xFF)); return *this; }
	W &Str(const char *s) { while (*s) Put((unsigned char)*s++); Put(0); return *this; }
};

static W Define(int menu, int serial, int layout, int flags, int items, int tabs, int headers, int pictures, int buttons,
	const char *title, int modelPaths = 0, int fields = 0, int sliders = 0, int toggles = 0)
{
	W w;
	w.Op(OP_DEFINE).Byte(menu).Short(serial).Byte(layout).Byte(flags).Short(items).Byte(tabs).Byte(headers)
	    .Byte(pictures).Byte(buttons).Byte(modelPaths).Byte(fields).Byte(sliders).Byte(toggles).Str(title);
	return w;
}

static bool Feed(Store &s, const W &w, std::vector<Event> *pEvents = nullptr, float now = 0.0f)
{
	std::vector<Event> ev;
	const bool ok = s.Parse(w.b.data(), w.b.size(), ev, now);
	if (pEvents)
		*pEvents = ev;
	return ok;
}

static W Costumes()
{
	W w = Define(2, 7, LAYOUT_GRID, MENU_SIDE | MENU_SEARCH | MENU_PREVIEW, 3, 2, 2, 0, 1, "Costumes", 2, 1, 1, 2);
	w.Op(OP_TOGGLE).Byte(2).Byte(0).Byte(1).Str("Radio on").Str("Hear the radio");
	w.Op(OP_TOGGLE).Byte(2).Byte(1).Byte(7).Str("Announce tracks").Str("");
	w.Op(OP_SLIDER).Byte(2).Byte(0).Short(0).Short(100).Short(5).Short(72).Str("Volume").Str("The radio's volume");
	w.Op(OP_FIELD).Byte(2).Byte(0).Byte(100).Str("Request").Str("Paste a link").Str("Add to queue");
	w.Op(OP_MODELPATH).Byte(2).Byte(0).Str("models/hats/sbo_hats_2025.mdl");
	w.Op(OP_MODELPATH).Byte(2).Byte(1).Str("models/costumes/costumes_packed_01.mdl");
	w.Op(OP_TAB).Byte(2).Byte(0).Str("Head");
	w.Op(OP_TAB).Byte(2).Byte(1).Str("Back");
	w.Op(OP_BUTTON).Byte(2).Byte(0).Str("Take off").Str("Take off what you wear");
	w.Op(OP_HEADER).Byte(2).Byte(0).Str("Brouzouf: 1200");
	w.Op(OP_ITEM).Byte(2).Short(0).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(ITEM_PICKED).Str("Santa").Str("")
	    .Str("Wearing").Str("Take off");
	w.Op(OP_ITEM).Byte(2).Short(1).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str("Angel Halo").Str("100")
	    .Str("").Str("Buy");
	w.Op(OP_ITEM).Byte(2).Short(2).Byte(1).Byte(PICTURE_NONE).Byte(0).Byte(ITEM_DISABLED).Str("Wings").Str("2400")
	    .Str("VIP").Str("Buy");
	w.Op(OP_ITEMTEXT).Byte(2).Short(1).Str("A golden ring");
	w.Op(OP_MODEL).Byte(2).Short(0).Byte(1).Byte(0).Short(3).Byte(1).Short(0).Short(MODEL_SAME).Short(MODEL_SAME);
	w.Op(OP_MODEL).Byte(2).Short(1).Byte(1).Byte(1).Short(37).Byte(0).Short(37).Short(92).Short(92);
	w.Op(OP_COOLDOWN).Byte(2).Short(2).Short(50).Short(100);
	w.Op(OP_AVATAR).Byte(2).Short(1).Long64(76561197962345041ull);
	return w;
}

static void TestRoundTrip()
{
	Store s;
	std::vector<Event> ev;
	W hello;
	hello.Op(OP_HELLO).Byte(1);
	CHECK(Feed(s, hello, &ev));
	CHECK(ev.size() == 1 && ev[0].kind == Event::HELLO && ev[0].a == 1);

	W def = Costumes();
	CHECK(def.b.size() <= (size_t)MSG_LIMIT * 3); // the plugin splits it; the parser takes any length
	CHECK(Feed(s, def, &ev));
	CHECK(!s.Get(2)); // pending until END
	W open;
	open.Op(OP_OPEN).Byte(2).Short(7).Byte(0).Short(1);
	CHECK(Feed(s, open, &ev));
	CHECK(ev.empty()); // not defined yet: ignored

	W end;
	end.Op(OP_END).Byte(2).Short(7);
	CHECK(Feed(s, end, &ev));
	CHECK(ev.size() == 1 && ev[0].kind == Event::DEFINED && ev[0].a == 2);
	const Menu *m = s.Get(2);
	CHECK(m != nullptr);
	if (!m)
		return;
	CHECK(m->title == "Costumes");
	CHECK(m->layout == LAYOUT_GRID);
	CHECK(m->flags == (MENU_SIDE | MENU_SEARCH | MENU_PREVIEW));
	CHECK(m->tabs.size() == 2 && m->tabs[1] == "Back");
	CHECK(m->buttons.size() == 1 && m->buttons[0] == "Take off");
	CHECK(m->buttonTips.size() == 1 && m->buttonTips[0] == "Take off what you wear");
	CHECK(m->headers[0] == "Brouzouf: 1200");
	CHECK(m->items.size() == 3);
	CHECK(m->items[0].defined && m->items[0].label == "Santa" && m->items[0].flags == ITEM_PICKED);
	CHECK(m->items[0].badge == "Wearing" && m->items[0].action == "Take off");
	CHECK(m->items[1].text == "A golden ring");
	CHECK(m->items[2].tab == 1 && m->items[2].flags == ITEM_DISABLED);
	CHECK(m->items[0].models[1].set && m->items[0].models[1].body == 3 && m->items[0].models[1].skin == 1);
	CHECK(m->items[0].models[1].body1 == 3 && m->items[0].models[1].sequence1 == 0);
	CHECK(m->items[1].models[1].body1 == 92 && m->items[1].models[1].sequence1 == 92);
	CHECK(!m->items[1].models[0].set);
	CHECK(m->ModelPath(m->items[0].models[1]) == "models/hats/sbo_hats_2025.mdl");
	CHECK(m->ModelPath(m->items[1].models[1]) == "models/costumes/costumes_packed_01.mdl");
	CHECK(m->ModelPath(m->items[1].models[0]).empty());
	CHECK(m->fields.size() == 1 && m->fields[0].label == "Request" && m->fields[0].hint == "Paste a link");
	CHECK(m->fields[0].button == "Add to queue" && m->fields[0].maxLength == 100);
	// 72 snaps to the step: 70
	CHECK(m->sliders.size() == 1 && m->sliders[0].label == "Volume" && m->sliders[0].value == 70);
	CHECK(m->sliders[0].min == 0 && m->sliders[0].max == 100 && m->sliders[0].step == 5);
	CHECK(m->toggles.size() == 2 && m->toggles[0].label == "Radio on" && m->toggles[0].on && m->toggles[1].on);
	CHECK(m->sliders[0].updates == 0 && m->toggles[0].updates == 0);
	CHECK(m->sliders[0].tip == "The radio's volume" && m->toggles[0].tip == "Hear the radio" && m->toggles[1].tip.empty());
	// the cooldown in the definition: 5 s of 10 s left from the parse's time (0)
	CHECK(m->items[2].CooldownLeft(0.0f) > 4.99f && m->items[2].CooldownLeft(0.0f) < 5.01f);
	CHECK(m->items[2].cooldownTotal > 9.99f && m->items[2].cooldownTotal < 10.01f);
	CHECK(m->items[2].CooldownLeft(5.0f) == 0.0f && m->items[1].CooldownLeft(0.0f) == 0.0f);
	CHECK(m->items[1].avatar == 76561197962345041ull && m->items[0].avatar == 0);

	CHECK(Feed(s, open, &ev));
	CHECK(ev.size() == 1 && ev[0].kind == Event::OPEN && ev[0].a == 2 && ev[0].b == 0 && ev[0].c == 1);
	W stale;
	stale.Op(OP_OPEN).Byte(2).Short(6).Byte(0).Short(1);
	CHECK(Feed(s, stale, &ev));
	CHECK(ev.empty());

	// live updates
	W live;
	live.Op(OP_HEADER).Byte(2).Byte(0).Str("Brouzouf: 1100");
	live.Op(OP_STATE).Byte(2).Short(1).Byte(ITEM_PICKED).Str("").Str("Owned").Str("Wear");
	live.Op(OP_VARIANT).Byte(2).Byte(1);
	CHECK(Feed(s, live, &ev));
	CHECK(ev.size() == 3);
	m = s.Get(2);
	CHECK(m->headers[0] == "Brouzouf: 1100");
	CHECK(m->items[1].badge == "Owned" && m->items[1].flags == ITEM_PICKED && m->items[1].action == "Wear");
	CHECK(m->variant == 1);

	// live label and cooldown: counted from the time the message arrives
	W live2;
	live2.Op(OP_LABEL).Byte(2).Short(1).Str("Angel Halo II");
	live2.Op(OP_COOLDOWN).Byte(2).Short(1).Short(120).Short(30); // a whole shorter than the time left: the time left
	live2.Op(OP_LABEL).Byte(2).Short(200).Str("past");
	live2.Op(OP_COOLDOWN).Byte(2).Short(200).Short(10).Short(10);
	CHECK(Feed(s, live2, &ev, 100.0f));
	CHECK(ev.size() == 2 && ev[0].kind == Event::CHANGED && ev[1].kind == Event::CHANGED);
	m = s.Get(2);
	CHECK(m->items[1].label == "Angel Halo II");
	CHECK(m->items[1].CooldownLeft(100.0f) > 11.99f && m->items[1].CooldownLeft(106.0f) < 6.01f);
	CHECK(m->items[1].cooldownTotal > 11.99f && m->items[1].cooldownTotal < 12.01f);
	CHECK(m->items[1].CooldownLeft(112.0f) == 0.0f);
	W clear;
	clear.Op(OP_COOLDOWN).Byte(2).Short(1).Short(0).Short(0);
	CHECK(Feed(s, clear, &ev, 101.0f));
	CHECK(s.Get(2)->items[1].CooldownLeft(101.0f) == 0.0f && s.Get(2)->items[1].cooldownEnd == 0.0f);

	// live slider value: clamped and snapped; past the sliders dropped
	W vol;
	vol.Op(OP_SLIDERVALUE).Byte(2).Byte(0).Short(33);
	vol.Op(OP_SLIDERVALUE).Byte(2).Byte(1).Short(10);
	CHECK(Feed(s, vol, &ev));
	CHECK(ev.size() == 1 && ev[0].kind == Event::CHANGED && s.Get(2)->sliders[0].value == 35);
	CHECK(s.Get(2)->sliders[0].updates == 1);
	// the server's answer to a flip (refused: the old state) and a toggle past the list
	W tog;
	tog.Op(OP_TOGGLEVALUE).Byte(2).Byte(0).Byte(0);
	tog.Op(OP_TOGGLEVALUE).Byte(2).Byte(5).Byte(1);
	CHECK(Feed(s, tog, &ev));
	CHECK(ev.size() == 1 && !s.Get(2)->toggles[0].on && s.Get(2)->toggles[0].updates == 1);
	// a live avatar, and none again
	W av;
	av.Op(OP_AVATAR).Byte(2).Short(0).Long64(0x0110000100000001ull);
	av.Op(OP_AVATAR).Byte(2).Short(1).Long64(0);
	av.Op(OP_AVATAR).Byte(2).Short(900).Long64(5);
	CHECK(Feed(s, av, &ev));
	CHECK(ev.size() == 2 && s.Get(2)->items[0].avatar == 0x0110000100000001ull && s.Get(2)->items[1].avatar == 0);
	W cuta;
	cuta.Op(OP_AVATAR).Byte(2).Short(0).Short(1).Short(2);
	CHECK(!Feed(s, cuta));
	// map pictures: live, lower-cased, a bad name gives none, past the items ignored, cut short fails
	W mp;
	mp.Op(OP_MAPPICTURE).Byte(2).Short(0).Str("CO_Omega17_v3");
	mp.Op(OP_MAPPICTURE).Byte(2).Short(1).Str("bk_city");
	mp.Op(OP_MAPPICTURE).Byte(2).Short(900).Str("br_x");
	CHECK(Feed(s, mp, &ev));
	CHECK(ev.size() == 2 && s.Get(2)->items[0].map == "co_omega17_v3" && s.Get(2)->items[1].map == "bk_city");
	W mpbad;
	mpbad.Op(OP_MAPPICTURE).Byte(2).Short(0).Str("../gfx/x");
	mpbad.Op(OP_MAPPICTURE).Byte(2).Short(1).Str("a/b");
	CHECK(Feed(s, mpbad, &ev));
	CHECK(s.Get(2)->items[0].map.empty() && s.Get(2)->items[1].map.empty());
	W mplong;
	mplong.Op(OP_MAPPICTURE).Byte(2).Short(0).Str("abcdefghijklmnopqrstuvwxyz0123456789"); // cut to LEN_MAP
	CHECK(Feed(s, mplong, &ev));
	CHECK(s.Get(2)->items[0].map.size() == (size_t)LEN_MAP);
	W mpnone;
	mpnone.Op(OP_MAPPICTURE).Byte(2).Short(0).Str("");
	CHECK(Feed(s, mpnone, &ev));
	CHECK(s.Get(2)->items[0].map.empty());
	W cutm;
	cutm.Op(OP_MAPPICTURE).Byte(2).Short(0);
	CHECK(!Feed(s, cutm));
	// a vote's share bar (past 100: none) and the popup's line, live
	W me;
	me.Op(OP_METER).Byte(2).Short(0).Byte(42);
	me.Op(OP_METER).Byte(2).Short(1).Byte(101);
	me.Op(OP_ITEMLINE).Byte(2).Short(0).Str("Co-op  Nominated by Kev");
	me.Op(OP_ITEMLINE).Byte(2).Short(900).Str("past the items");
	CHECK(Feed(s, me, &ev));
	CHECK(ev.size() == 3 && s.Get(2)->items[0].meter == 42 && s.Get(2)->items[1].meter == METER_NONE &&
	      s.Get(2)->items[0].line == "Co-op  Nominated by Kev");
	W cutme;
	cutme.Op(OP_METER).Byte(2).Short(0);
	CHECK(!Feed(s, cutme));
	W cutil;
	cutil.Op(OP_ITEMLINE).Byte(2).Short(0).Byte('a');
	CHECK(!Feed(s, cutil));
	// a live description (a queue row's text and link change with its track)
	W tx;
	tx.Op(OP_ITEMTEXT).Byte(2).Short(1).Str("Now: https://example.com/watch?v=2");
	tx.Op(OP_ITEMTEXT).Byte(2).Short(900).Str("past the items");
	CHECK(Feed(s, tx, &ev));
	CHECK(ev.size() == 1 && ev[0].kind == Event::CHANGED && s.Get(2)->items[1].text == "Now: https://example.com/watch?v=2");
	// the popup's full title (past the label's length), live; cut short fails
	const std::string longTitle = "Donkey Kong Country OST (Super Nintendo) - Track 07 - Aquatic Ambience (Extended)";
	W tt;
	tt.Op(OP_ITEMTITLE).Byte(2).Short(1).Str(longTitle.c_str());
	tt.Op(OP_ITEMTITLE).Byte(2).Short(900).Str("past");
	CHECK(Feed(s, tt, &ev));
	CHECK(ev.size() == 1 && s.Get(2)->items[1].title == longTitle && longTitle.size() > (size_t)LEN_LABEL_V2);
	W ttcut;
	ttcut.Op(OP_ITEMTITLE).Byte(2).Short(1);
	CHECK(!Feed(s, ttcut));
	// a description keeps its line breaks (lines one under another), drops the other control characters
	W tx2;
	tx2.Op(OP_ITEMTEXT).Byte(2).Short(1).Str("Line one\nLine\x01 two\r");
	CHECK(Feed(s, tx2, &ev));
	CHECK(s.Get(2)->items[1].text == "Line one\nLine two");
	// badges with tooltips and voter bubbles, live; bad styles plain, bad client indices dropped, too many fails
	W bd;
	bd.Op(OP_BADGES).Byte(2).Short(0).Byte(2).Byte(BADGE_GREEN).Str("Your vote").Str("You voted for this map")
	    .Byte(9).Str("Winning").Str("43% of the votes");
	bd.Op(OP_VOTERS).Byte(2).Short(0).Byte(4).Byte(1).Byte(7).Byte(0).Byte(40);
	CHECK(Feed(s, bd, &ev));
	CHECK(ev.size() == 2 && s.Get(2)->items[0].badges.size() == 2 && s.Get(2)->items[0].badges[0].style == BADGE_GREEN &&
	      s.Get(2)->items[0].badges[1].style == BADGE_PLAIN && s.Get(2)->items[0].badges[1].tip == "43% of the votes" &&
	      s.Get(2)->items[0].voters.size() == 2 && s.Get(2)->items[0].voters[1] == 7);
	W bd0;
	bd0.Op(OP_BADGES).Byte(2).Short(0).Byte(0);
	bd0.Op(OP_VOTERS).Byte(2).Short(0).Byte(0);
	CHECK(Feed(s, bd0, &ev));
	CHECK(s.Get(2)->items[0].badges.empty() && s.Get(2)->items[0].voters.empty());
	W bdmany;
	bdmany.Op(OP_BADGES).Byte(2).Short(0).Byte(4);
	for (int b = 0; b < 4; b++)
		bdmany.Byte(0).Str("x").Str("y");
	CHECK(!Feed(s, bdmany));
	W vmany;
	vmany.Op(OP_VOTERS).Byte(2).Short(0).Byte(MAX_VOTERS + 1);
	for (int v = 0; v <= MAX_VOTERS; v++)
		vmany.Byte(1);
	CHECK(!Feed(s, vmany));
	W cutbd;
	cutbd.Op(OP_BADGES).Byte(2).Short(0).Byte(1).Byte(0).Str("x");
	CHECK(!Feed(s, cutbd));
	W cutv2;
	cutv2.Op(OP_VOTERS).Byte(2).Short(0).Byte(3).Byte(1);
	CHECK(!Feed(s, cutv2));
	CHECK(ValidMapName("co_sbotest") && ValidMapName("rc_kart-2.0") && !ValidMapName("Co_x") && !ValidMapName("")
	      && !ValidMapName("a..b") && !ValidMapName("a b"));
	W cutt;
	cutt.Op(OP_TOGGLEVALUE).Byte(2).Byte(0);
	CHECK(!Feed(s, cutt));
	W loud;
	loud.Op(OP_SLIDERVALUE).Byte(2).Byte(0).Short(65535);
	CHECK(Feed(s, loud, &ev));
	CHECK(s.Get(2)->sliders[0].value == 100);
	W cutv;
	cutv.Op(OP_SLIDERVALUE).Byte(2).Byte(0);
	CHECK(!Feed(s, cutv));

	// a redefinition: the old one stays until the new END
	W redef = Define(2, 8, LAYOUT_LIST, 0, 1, 0, 0, 0, 0, "Costumes 2");
	redef.Op(OP_ITEM).Byte(2).Short(0).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str("Only").Str("").Str("")
	    .Str("");
	CHECK(Feed(s, redef, &ev));
	CHECK(s.Get(2) && s.Get(2)->serial == 7);
	W end8;
	end8.Op(OP_END).Byte(2).Short(8);
	CHECK(Feed(s, end8, &ev));
	CHECK(s.Get(2) && s.Get(2)->serial == 8 && s.Get(2)->items.size() == 1);

	W close;
	close.Op(OP_CLOSE).Byte(CLOSE_ANY);
	CHECK(Feed(s, close, &ev));
	CHECK(ev.size() == 1 && ev[0].kind == Event::CLOSE && ev[0].a == CLOSE_ANY);

	s.Reset();
	CHECK(!s.Get(2));
}

static void TestHostile()
{
	Store s;
	std::vector<Event> ev;

	// counts past the caps: the definition is refused, the rest of the message dropped
	CHECK(!Feed(s, Define(0, 1, 0, 0, MAX_MENU_ITEMS + 1, 0, 0, 0, 0, "x")));
	CHECK(!Feed(s, Define(0, 1, 0, 0, 1, MAX_TABS + 1, 0, 0, 0, "x")));
	CHECK(!Feed(s, Define(0, 1, 0, 0, 1, 0, 0, MAX_PICTURES + 1, 0, "x")));
	CHECK(!Feed(s, Define(0, 1, 0, 0, 1, 0, 0, 0, MAX_MENU_BUTTONS + 1, "x")));
	CHECK(!Feed(s, Define(MAX_SERVER_MENUS, 1, 0, 0, 1, 0, 0, 0, 0, "x")));
	CHECK(!Feed(s, Define(0, 1, 0, 0, 1, 0, 0, 0, 0, "x", MAX_MODEL_PATHS + 1)));
	CHECK(!Feed(s, Define(0, 1, 0, 0, 1, 0, 0, 0, 0, "x", 0, MAX_MENU_FIELDS + 1)));
	CHECK(!Feed(s, Define(0, 1, 0, 0, 1, 0, 0, 0, 0, "x", 0, 0, MAX_MENU_SLIDERS + 1)));
	CHECK(!Feed(s, Define(0, 1, 0, 0, 1, 0, 0, 0, 0, "x", 0, 0, 0, MAX_MENU_TOGGLES + 1)));
	CHECK(!s.LastError().empty());
	CHECK(s.TakeErrorToReport());
	CHECK(!s.TakeErrorToReport()); // once per map

	// indices past the definition: dropped, the rest still parsed
	W w = Define(1, 3, 9 /* bad layout */, 0xFF, 2, 1, 1, 1, 0, "Hostile", 2, 1, 1, 1);
	w.Op(OP_TOGGLE).Byte(1).Byte(0).Byte(1).Str(std::string(90, 'T').c_str()).Str(std::string(90, 't').c_str());
	w.Op(OP_TOGGLE).Byte(1).Byte(3).Byte(1).Str("past").Str("");
	w.Op(OP_SLIDER).Byte(1).Byte(0).Short(500).Short(10).Short(0).Short(9).Str(std::string(60, 'S').c_str()).Str("");
	w.Op(OP_SLIDER).Byte(1).Byte(1).Short(0).Short(1).Short(1).Short(0).Str("past").Str("");
	w.Op(OP_FIELD).Byte(1).Byte(0).Byte(0).Str(std::string(80, 'L').c_str()).Str("h\x01int").Str("Go");
	w.Op(OP_FIELD).Byte(1).Byte(1).Byte(255).Str("past").Str("").Str("");
	w.Op(OP_MODELPATH).Byte(1).Byte(0).Str("C:/x.mdl");
	w.Op(OP_MODELPATH).Byte(1).Byte(1).Str("models/ok.mdl");
	w.Op(OP_MODELPATH).Byte(1).Byte(2).Str("models/past.mdl");
	w.Op(OP_TAB).Byte(1).Byte(5).Str("past");
	w.Op(OP_PICTURE).Byte(1).Byte(0).Byte(0).Byte(200).Str("../../evil.tga");
	w.Op(OP_ITEM).Byte(1).Short(60000).Byte(0).Byte(0).Byte(0).Byte(0).Str("past").Str("").Str("").Str("");
	w.Op(OP_ITEM).Byte(1).Short(0).Byte(200).Byte(9).Byte(250).Byte(0xFF).Str("ok").Str("").Str("").Str("");
	w.Op(OP_HEADER).Byte(1).Byte(9).Str("past");
	w.Op(OP_MODEL).Byte(1).Short(0).Byte(9).Byte(1).Short(0).Byte(0).Short(0).Short(0).Short(0);
	w.Op(OP_MODEL).Byte(1).Short(0).Byte(1).Byte(0).Short(0).Byte(0).Short(0).Short(0).Short(0); // refused path
	w.Op(OP_MODEL).Byte(1).Short(0).Byte(2).Byte(2).Short(0).Byte(0).Short(0).Short(0).Short(0); // past the table
	w.Op(OP_MODEL).Byte(1).Short(0).Byte(3).Byte(1).Short(65535).Byte(255).Short(65535).Short(0).Short(0);
	w.Op(OP_COOLDOWN).Byte(1).Short(0).Short(65535).Short(65535);
	w.Op(OP_COOLDOWN).Byte(1).Short(1).Short(10).Short(10); // not defined: dropped
	w.Op(OP_END).Byte(1).Short(3);
	CHECK(Feed(s, w, &ev));
	const Menu *m = s.Get(1);
	CHECK(m != nullptr);
	if (m)
	{
		CHECK(m->layout == LAYOUT_LIST);
		CHECK(m->flags == MENU_FLAGS_ALL);
		CHECK(m->tabs[0].empty());
		CHECK(m->pictures[0].path.empty() && m->pictures[0].columns == 1 && m->pictures[0].rows == MAX_SHEET_CELLS);
		CHECK(m->items[0].defined && m->items[0].tab == 0 && m->items[0].picture == PICTURE_NONE);
		CHECK(m->items[0].flags == ITEM_FLAGS_ALL);
		CHECK(!m->items[0].models[1].set && !m->items[0].models[2].set);
		CHECK(m->items[0].models[3].set && m->ModelPath(m->items[0].models[3]) == "models/ok.mdl");
		CHECK(m->modelPaths[0].empty());
		CHECK(m->fields.size() == 1 && m->fields[0].label.size() == (size_t)LEN_FIELD_LABEL);
		CHECK(m->fields[0].hint == "hint" && m->fields[0].maxLength == 1);
		// max under min: max = min; step 0: 1; the label cut
		CHECK(m->sliders.size() == 1 && m->sliders[0].min == 500 && m->sliders[0].max == 500);
		CHECK(m->sliders[0].step == 1 && m->sliders[0].value == 500);
		CHECK(m->sliders[0].label.size() == (size_t)LEN_SLIDER_LABEL);
		CHECK(m->toggles.size() == 1 && m->toggles[0].on && m->toggles[0].label.size() == (size_t)LEN_TOGGLE_LABEL);
		CHECK(m->items[0].CooldownLeft(0.0f) > 6553.0f && m->items[0].CooldownLeft(0.0f) < 6554.0f);
		CHECK(!m->items[1].defined && m->items[1].cooldownEnd == 0.0f);
	}

	// a string without its end
	W nul;
	nul.Op(OP_HEADER).Byte(1).Byte(0);
	for (int i = 0; i < 100; i++)
		nul.Byte('A');
	CHECK(!Feed(s, nul));
	CHECK(s.Get(1)->headers[0].empty());

	// control characters and long strings
	W ctl;
	ctl.Op(OP_HEADER).Byte(1).Byte(0).Str("a\x01\x1b[31mb\x7f");
	CHECK(Feed(s, ctl));
	CHECK(s.Get(1)->headers[0] == "a[31mb");
	std::string longText(200, 'x'); // a record's length is a byte
	W lng;
	lng.Op(OP_HEADER).Byte(1).Byte(0).Str(longText.c_str());
	CHECK(Feed(s, lng));
	CHECK(s.Get(1)->headers[0].size() == (size_t)LEN_HEADER);
	// a cut inside a UTF-8 character backs off to its start
	std::string utf;
	for (int i = 0; i < 40; i++)
		utf += "\xC3\xA9"; // e acute, 2 bytes
	W u;
	u.Op(OP_HEADER).Byte(1).Byte(0).Str(utf.c_str());
	CHECK(Feed(s, u));
	CHECK(s.Get(1)->headers[0].size() == 80);
	CHECK(CleanString(utf.c_str(), utf.size(), 5).size() == 4);

	// an op this client does not know (a newer server's): skipped, the next record still read
	W unk;
	unk.Op(99).Byte(1).Byte(2).Str("from a newer server");
	unk.Op(OP_HEADER).Byte(1).Byte(1).Str("after it");
	CHECK(Feed(s, unk));
	CHECK(s.Get(1)->headers[1] == "after it");
	// fields a newer server appends to a known record: skipped
	W more;
	more.Op(OP_HEADER).Byte(1).Byte(1).Str("known").Short(7).Str("appended");
	more.Op(OP_HEADER).Byte(1).Byte(2).Str("next");
	CHECK(Feed(s, more));
	CHECK(s.Get(1)->headers[1] == "known" && s.Get(1)->headers[2] == "next");
	// a record that reads past its own length: dropped alone, the next one read
	W shortRec;
	shortRec.Op(OP_HEADER).Byte(1); // no line, no text
	shortRec.Op(OP_HEADER).Byte(1).Byte(3).Str("still read");
	CHECK(!Feed(s, shortRec));
	CHECK(s.Get(1)->headers[3] == "still read");
	// a length past the message's end: the message stops there
	W cutLen;
	cutLen.Op(OP_HEADER).Byte(1).Byte(0).Str("whole");
	cutLen.b.push_back(OP_HEADER);
	cutLen.b.push_back(40);
	cutLen.b.push_back(1);
	std::vector<Event> cutEv;
	CHECK(!Feed(s, cutLen, &cutEv));
	CHECK(s.Get(1)->headers[0] == "whole");
	W noLen;
	noLen.b.push_back(OP_HEADER);
	CHECK(!Feed(s, noLen));

	// versions: a newer server's HELLO is read (the client answers with its own); a version 1 HELLO (no length)
	// is not, so the two never get past the hello
	W hello9;
	hello9.Op(OP_HELLO).Byte(9).Byte(1).Str("newer fields");
	std::vector<Event> hev;
	CHECK(Feed(s, hello9, &hev));
	CHECK(hev.size() == 1 && hev[0].kind == Event::HELLO && hev[0].a == 9);
	W hello1;
	hello1.b.push_back(OP_HELLO);
	hello1.b.push_back(1);
	CHECK(!Feed(s, hello1, &hev));
	CHECK(hev.empty());

	// END of another serial drops the pending definition
	W d2 = Define(3, 10, 0, 0, 1, 0, 0, 0, 0, "x");
	d2.Op(OP_END).Byte(3).Short(11);
	Feed(s, d2);
	CHECK(!s.Get(3));

	// the text limit: definitions until the store is full, never past MAX_TEXT_BYTES
	Store full;
	std::string label(LEN_LABEL, 'L'), price(LEN_PRICE, 'P'), badge(LEN_BADGE, 'B'), action(LEN_ACTION, 'A');
	std::string text(LEN_TEXT, 'T');
	for (int menu = 0; menu < MAX_SERVER_MENUS; menu++)
	{
		Feed(full, Define(menu, 1, 0, 0, MAX_MENU_ITEMS, 0, 0, 0, 0, "full"));
		for (int i = 0; i < MAX_MENU_ITEMS; i++)
		{
			W it;
			it.Op(OP_ITEM).Byte(menu).Short(i).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str(label.c_str())
			    .Str(price.c_str()).Str(badge.c_str()).Str(action.c_str());
			it.Op(OP_ITEMTEXT).Byte(menu).Short(i).Str(text.c_str());
			Feed(full, it);
			CHECK(full.TextBytes() <= (size_t)MAX_TEXT_BYTES);
		}
		W e;
		e.Op(OP_END).Byte(menu).Short(1);
		Feed(full, e);
	}
	CHECK(full.TextBytes() <= (size_t)MAX_TEXT_BYTES);

	// a live MAPPICTURE / ITEMLINE over the limit is dropped alone: the rest of the message still applies
	CHECK(full.Get(0) != nullptr);
	for (int menu = 1; menu < MAX_SERVER_MENUS && (size_t)MAX_TEXT_BYTES - full.TextBytes() > 8; menu++)
	{
		Feed(full, Define(menu, 2, 0, 0, MAX_MENU_ITEMS, 0, 0, 0, 0, ""));
		for (int i = 0; i < MAX_MENU_ITEMS && (size_t)MAX_TEXT_BYTES - full.TextBytes() > 8; i++)
		{
			const size_t room = (size_t)MAX_TEXT_BYTES - full.TextBytes() - 4;
			W t;
			t.Op(OP_ITEMTEXT).Byte(menu).Short(i).Str(std::string(std::min(room, (size_t)LEN_TEXT), 'T').c_str());
			Feed(full, t);
		}
	}
	CHECK((size_t)MAX_TEXT_BYTES - full.TextBytes() <= 8);
	W over;
	over.Op(OP_MAPPICTURE).Byte(0).Short(0).Str("co_a_rather_long_map_name_here");
	over.Op(OP_ITEMLINE).Byte(0).Short(0).Str("a line longer than the bytes the store has left");
	over.Op(OP_METER).Byte(0).Short(0).Byte(50);
	std::vector<Event> overEv;
	CHECK(Feed(full, over, &overEv));
	CHECK(full.Get(0)->items[0].map.empty() && full.Get(0)->items[0].line.empty());
	CHECK(full.Get(0)->items[0].meter == 50 && overEv.size() == 1 && overEv[0].kind == Event::CHANGED);
	CHECK(full.TextBytes() <= (size_t)MAX_TEXT_BYTES);
}

//! The HUD strip and the text field's text
static void TestHudAndText()
{
	Store s;
	std::vector<Event> ev;
	W show;
	show.Op(OP_HUDBAR).Byte(HUDBAR_SHOW).Short(30).Short(271).Short(10).Long64(76561197962345041ull).Long64(76561197960287930ull).Str("Track title").Str("Requested by X")
	    .Str("Next track");
	CHECK(Feed(s, show, &ev, 100.0f));
	CHECK(ev.size() == 1 && ev[0].kind == Event::HUDBAR);
	const HudBar &h = s.Hud();
	CHECK(h.shown && h.title == "Track title" && h.sub == "Requested by X" && h.next == "Next track");
	CHECK(h.start == 70.0f && h.length == 271.0f && h.nextShown && h.nextUntil == 110.0f);
	CHECK(h.avatar == 76561197962345041ull && h.nextAvatar == 76561197960287930ull);
	CHECK(show.b.size() <= (size_t)MSG_BYTES);
	// played past the end: the bar is full, never past it
	W over;
	over.Op(OP_HUDBAR).Byte(HUDBAR_SHOW | HUDBAR_NEXT_GONE).Short(500).Short(60).Short(0).Long64(0).Long64(0).Str("T").Str("").Str("N");
	CHECK(Feed(s, over, &ev, 100.0f));
	CHECK(s.Hud().start == 40.0f && !s.Hud().nextShown && s.Hud().nextUntil == 0.0f);
	// a notice in the extra line (no "Up next" label)
	W note;
	note.Op(OP_HUDBAR).Byte(HUDBAR_SHOW | HUDBAR_NOTICE).Short(0).Short(0).Short(5).Long64(0).Long64(0).Str("T").Str("")
	    .Str("Song not queued: the queue is full");
	CHECK(Feed(s, note, &ev, 100.0f));
	CHECK(s.Hud().notice && s.Hud().nextShown && s.Hud().next == "Song not queued: the queue is full");
	// no length: no bar, the up-next line stays
	W live;
	live.Op(OP_HUDBAR).Byte(HUDBAR_SHOW).Short(0).Short(0).Short(0).Long64(0).Long64(0).Str("Live").Str("").Str("Later");
	CHECK(Feed(s, live, &ev, 5.0f));
	CHECK(s.Hud().length == 0.0f && s.Hud().nextShown && s.Hud().nextUntil == 0.0f);
	W hide;
	hide.Op(OP_HUDBAR).Byte(0).Short(0).Short(0).Short(0).Long64(9).Long64(9).Str("x").Str("y").Str("z");
	CHECK(Feed(s, hide, &ev));
	CHECK(!s.Hud().shown && s.Hud().title.empty() && s.Hud().avatar == 0 && s.Hud().nextAvatar == 0);
	// the longest strip fits the message cap
	W longest;
	longest.Op(OP_HUDBAR).Byte(HUDBAR_SHOW).Short(1).Short(2).Short(3).Long64(1).Long64(1)
	    .Str(std::string(LEN_HUD_TITLE, 'T').c_str()).Str(std::string(LEN_HUD_LINE, 'S').c_str())
	    .Str(std::string(LEN_HUD_LINE, 'N').c_str());
	CHECK(longest.b.size() <= (size_t)MSG_BYTES);
	CHECK(Feed(s, longest, &ev));
	W cut;
	cut.Op(OP_HUDBAR).Byte(HUDBAR_SHOW).Short(0).Short(0);
	CHECK(!Feed(s, cut));
	CHECK(CleanFieldText("  https://x.y/watch?v=1 ", LEN_FIELD_TEXT) == "https://x.y/watch?v=1");
	CHECK(CleanFieldText("a\"; quit\x01\n\\b", LEN_FIELD_TEXT) == "a quitb");
	CHECK(CleanFieldText("   ", LEN_FIELD_TEXT).empty());
	CHECK(CleanFieldText(std::string(300, 'x'), LEN_FIELD_TEXT).size() == (size_t)LEN_FIELD_TEXT);
	std::string utf;
	for (int i = 0; i < 100; i++)
		utf += "\xC3\xA9";
	CHECK(CleanFieldText(utf, LEN_FIELD_TEXT).size() == 126);
	s.Reset();
	CHECK(!s.Hud().shown);
}

static void TestPaths()
{
	CHECK(ValidPicturePath("gfx/hats/hats_sheet.tga"));
	CHECK(ValidPicturePath("sprites/shop-1.tga"));
	CHECK(!ValidPicturePath("gfx/../config.cfg"));
	CHECK(!ValidPicturePath("../gfx/a.tga"));
	CHECK(!ValidPicturePath("/gfx/a.tga"));
	CHECK(!ValidPicturePath("gfx//a.tga"));
	CHECK(!ValidPicturePath("gfx/a.b.tga"));
	CHECK(!ValidPicturePath("gfx/A.tga"));
	CHECK(!ValidPicturePath("gfx/a.bmp"));
	CHECK(!ValidPicturePath("gfx\\a.tga"));
	CHECK(!ValidPicturePath("c:gfx/a.tga"));
	CHECK(!ValidPicturePath("gfx/%s.tga"));
	CHECK(!ValidPicturePath("models/a.tga"));
	CHECK(!ValidPicturePath("gfx/.tga"));
	CHECK(!ValidPicturePath("gfx/" + std::string(60, 'a') + ".tga"));
	CHECK(ValidModelPath("models/player/gina/gina.mdl"));
	CHECK(ValidModelPath("models/player/Barney2/Barney2.mdl"));
	CHECK(ValidModelPath("models/costumes/costumes_packed_01.mdl"));
	CHECK(!ValidModelPath("models/../../hl.so"));
	CHECK(!ValidModelPath("maps/a.mdl"));
	CHECK(!ValidModelPath("models/a.spr"));
	CHECK(!ValidModelPath("models/a b.mdl"));
}

//! Dropdowns (SELECT, SELECTOPTION, SELECTVALUE), confirm questions (CONFIRM) and item pictures (ITEMPICTURE)
static W Shop()
{
	W w = Define(3, 11, LAYOUT_GRID, MENU_POPUP, 2, 0, 0, 0, 1, "Shop");
	w.Op(OP_ITEM).Byte(3).Short(0).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str("Rocket").Str("300").Str("")
	    .Str("Buy");
	w.Op(OP_ITEM).Byte(3).Short(1).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str("Loadout").Str("").Str("").Str("");
	w.Op(OP_BUTTON).Byte(3).Byte(0).Str("Sell all").Str("");
	w.Op(OP_SELECT).Byte(3).Byte(0).Short(NO_ITEM).Byte(3).Byte(1).Str("Difficulty").Str("How hard");
	w.Op(OP_SELECTOPTION).Byte(3).Byte(0).Byte(0).Str("Easy");
	w.Op(OP_SELECTOPTION).Byte(3).Byte(0).Byte(1).Str("Normal");
	w.Op(OP_SELECTOPTION).Byte(3).Byte(0).Byte(2).Str("Hard");
	w.Op(OP_SELECTOPTION).Byte(3).Byte(0).Byte(3).Str("past the options");
	w.Op(OP_SELECT).Byte(3).Byte(2).Short(1).Byte(2).Byte(5).Str("Loadout").Str("");
	w.Op(OP_SELECTOPTION).Byte(3).Byte(2).Byte(0).Str("Federal Rifle");
	w.Op(OP_SELECTOPTION).Byte(3).Byte(2).Byte(1).Str("Shotgun");
	w.Op(OP_CONFIRM).Byte(3).Byte(CONFIRM_ITEM).Short(0).Str("Buy Rocket for 300?");
	w.Op(OP_CONFIRM).Byte(3).Byte(CONFIRM_BUTTON).Short(0).Str("Sell everything?");
	w.Op(OP_ITEMPICTURE).Byte(3).Short(0).Str("gfx/shop/rocket.tga");
	w.Op(OP_END).Byte(3).Short(11);
	return w;
}

static void TestSelectConfirmPicture()
{
	Store s;
	std::vector<Event> ev;
	W shop = Shop();
	CHECK(Feed(s, shop, &ev));
	const Menu *m = s.Get(3);
	CHECK(m != nullptr);
	if (!m)
		return;
	CHECK(m->selects.size() == 3);
	CHECK(m->selects[0].defined && m->selects[0].item == NO_ITEM && m->selects[0].label == "Difficulty");
	CHECK(m->selects[0].tip == "How hard" && m->selects[0].options.size() == 3 && m->selects[0].value == 1);
	CHECK(m->selects[0].options[2] == "Hard");
	CHECK(!m->selects[1].defined); // never sent: not shown
	CHECK(m->selects[2].defined && m->selects[2].item == 1 && m->selects[2].value == 0); // 5 is past its options
	CHECK(m->selects[2].options[0] == "Federal Rifle" && m->selects[2].options[1] == "Shotgun");
	CHECK(m->items[0].confirm == "Buy Rocket for 300?" && m->items[1].confirm.empty());
	CHECK(m->buttonConfirms.size() == 1 && m->buttonConfirms[0] == "Sell everything?");
	CHECK(m->items[0].image == "gfx/shop/rocket.tga" && m->items[1].image.empty());
	CHECK(m->buttonFlags.size() == 1 && m->buttonFlags[0] == 0 && m->ButtonEnabled(0));
	CHECK(!m->ButtonEnabled(1) && !m->ButtonEnabled(-1));

	// live: the choice, the questions and the picture
	W live;
	live.Op(OP_SELECTVALUE).Byte(3).Byte(0).Byte(2);
	live.Op(OP_SELECTVALUE).Byte(3).Byte(0).Byte(3);  // past its options: dropped
	live.Op(OP_SELECTVALUE).Byte(3).Byte(1).Byte(0);  // not defined: dropped
	live.Op(OP_SELECTVALUE).Byte(3).Byte(9).Byte(0);  // past the selects: dropped
	live.Op(OP_CONFIRM).Byte(3).Byte(CONFIRM_ITEM).Short(0).Str("");
	live.Op(OP_CONFIRM).Byte(3).Byte(CONFIRM_ITEM).Short(1).Str("Change loadout?");
	live.Op(OP_CONFIRM).Byte(3).Byte(CONFIRM_BUTTON).Short(1).Str("past the buttons");
	live.Op(OP_CONFIRM).Byte(3).Byte(7).Short(0).Str("bad kind");
	live.Op(OP_ITEMPICTURE).Byte(3).Short(1).Str("gfx/shop/loadout.tga");
	live.Op(OP_ITEMPICTURE).Byte(3).Short(0).Str("gfx/../config.tga"); // a bad path: no picture
	live.Op(OP_BUTTONSTATE).Byte(3).Byte(0).Byte(0xFF);  // greyed (unknown flag bits dropped)
	live.Op(OP_BUTTONSTATE).Byte(3).Byte(1).Byte(BUTTON_DISABLED); // past the buttons: dropped
	CHECK(Feed(s, live, &ev));
	m = s.Get(3);
	CHECK(m->selects[0].value == 2 && m->selects[0].updates == 1);
	CHECK(m->items[0].confirm.empty() && m->items[1].confirm == "Change loadout?");
	CHECK(m->buttonConfirms.size() == 1 && m->buttonConfirms[0] == "Sell everything?");
	CHECK(m->items[1].image == "gfx/shop/loadout.tga" && m->items[0].image.empty());
	CHECK(m->buttonFlags.size() == 1 && m->buttonFlags[0] == BUTTON_DISABLED && !m->ButtonEnabled(0));
	int changed = 0;
	for (const Event &e : ev)
		changed += e.kind == Event::CHANGED && e.a == 3;
	CHECK(changed == 6);

	// enabled again live; a new definition starts with every button enabled
	W again;
	again.Op(OP_BUTTONSTATE).Byte(3).Byte(0).Byte(0);
	again.Op(OP_BUTTONSTATE).Byte(3).Byte(0); // short: dropped, the message goes on
	CHECK(!Feed(s, again));
	CHECK(s.Get(3)->ButtonEnabled(0));
	W off;
	off.Op(OP_BUTTONSTATE).Byte(3).Byte(0).Byte(BUTTON_DISABLED);
	CHECK(Feed(s, off));
	CHECK(!s.Get(3)->ButtonEnabled(0));
	W redefine = Shop();
	CHECK(Feed(s, redefine));
	CHECK(s.Get(3)->ButtonEnabled(0));
	// a BUTTONSTATE inside a definition holds for it
	W pend = Define(5, 2, LAYOUT_LIST, 0, 1, 0, 0, 0, 2, "Vote");
	pend.Op(OP_ITEM).Byte(5).Short(0).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str("a").Str("").Str("").Str("");
	pend.Op(OP_BUTTON).Byte(5).Byte(0).Str("None").Str("");
	pend.Op(OP_BUTTON).Byte(5).Byte(1).Str("Undo my vote").Str("");
	pend.Op(OP_BUTTONSTATE).Byte(5).Byte(1).Byte(BUTTON_DISABLED);
	pend.Op(OP_END).Byte(5).Short(2);
	CHECK(Feed(s, pend));
	CHECK(s.Get(5) && s.Get(5)->ButtonEnabled(0) && !s.Get(5)->ButtonEnabled(1));

	// outside a definition SELECT and SELECTOPTION are dropped; caps and bad counts too
	W late;
	late.Op(OP_SELECT).Byte(3).Byte(1).Short(NO_ITEM).Byte(2).Byte(0).Str("Late").Str("");
	late.Op(OP_SELECTOPTION).Byte(3).Byte(0).Byte(0).Str("Changed");
	CHECK(Feed(s, late));
	m = s.Get(3);
	CHECK(!m->selects[1].defined && m->selects[0].options[0] == "Easy");
	W caps = Define(4, 1, LAYOUT_LIST, 0, 1, 0, 0, 0, 0, "Caps");
	caps.Op(OP_ITEM).Byte(4).Short(0).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str("a").Str("").Str("").Str("");
	caps.Op(OP_SELECT).Byte(4).Byte(MAX_MENU_SELECTS).Short(NO_ITEM).Byte(2).Byte(0).Str("past").Str("");
	caps.Op(OP_SELECT).Byte(4).Byte(0).Short(NO_ITEM).Byte(0).Byte(0).Str("no options").Str("");
	caps.Op(OP_SELECT).Byte(4).Byte(1).Short(NO_ITEM).Byte(MAX_SELECT_OPTIONS + 1).Byte(0).Str("too many").Str("");
	caps.Op(OP_SELECT).Byte(4).Byte(2).Short(5).Byte(2).Byte(0).Str("no such item").Str("");
	caps.Op(OP_SELECT).Byte(4).Byte(3).Short(0).Byte(MAX_SELECT_OPTIONS).Byte(0)
	    .Str(std::string(80, 'L').c_str()).Str(std::string(90, 't').c_str());
	caps.Op(OP_SELECTOPTION).Byte(4).Byte(3).Byte(MAX_SELECT_OPTIONS - 1).Str(std::string(80, 'o').c_str());
	caps.Op(OP_CONFIRM).Byte(4).Byte(CONFIRM_ITEM).Short(0).Str(std::string(200, 'q').c_str());
	caps.Op(OP_END).Byte(4).Short(1);
	CHECK(Feed(s, caps));
	m = s.Get(4);
	CHECK(m != nullptr);
	if (m)
	{
		CHECK(m->selects.size() == 4);
		CHECK(!m->selects[0].defined && !m->selects[1].defined && !m->selects[2].defined);
		CHECK(m->selects[3].defined && m->selects[3].options.size() == (size_t)MAX_SELECT_OPTIONS);
		CHECK(m->selects[3].label.size() == (size_t)LEN_SELECT_LABEL && m->selects[3].tip.size() == (size_t)LEN_CONTROL_TIP);
		CHECK(m->selects[3].options.back().size() == (size_t)LEN_OPTION);
		CHECK(m->items[0].confirm.size() == (size_t)LEN_CONFIRM);
	}
	// every record fits a message
	W one;
	one.Op(OP_SELECT).Byte(0).Byte(0).Short(0).Byte(16).Byte(0).Str(std::string(LEN_SELECT_LABEL, 'a').c_str())
	    .Str(std::string(LEN_CONTROL_TIP, 'b').c_str());
	CHECK(one.b.size() <= (size_t)MSG_BYTES);
	W two;
	two.Op(OP_CONFIRM).Byte(0).Byte(0).Short(0).Str(std::string(LEN_CONFIRM, 'c').c_str());
	CHECK(two.b.size() <= (size_t)MSG_BYTES);

	// every prefix, every byte changed
	for (size_t n = 0; n <= shop.b.size(); n++)
	{
		Store t;
		unsigned char *copy = (unsigned char *)malloc(n ? n : 1);
		memcpy(copy, shop.b.data(), n);
		t.Parse(copy, n, ev);
		free(copy);
	}
	for (size_t i = 0; i < shop.b.size(); i++)
	{
		for (int v = 0; v < 256; v += 17)
		{
			Store t;
			std::vector<unsigned char> b = shop.b;
			b[i] = (unsigned char)v;
			unsigned char *copy = (unsigned char *)malloc(b.size());
			memcpy(copy, b.data(), b.size());
			ev.clear();
			t.Parse(copy, b.size(), ev);
			free(copy);
			const Menu *menu = t.Get(3);
			if (!menu)
				continue;
			for (const Select &sel : menu->selects)
			{
				CHECK(sel.options.size() <= (size_t)MAX_SELECT_OPTIONS);
				CHECK(!sel.defined || (sel.value >= 0 && sel.value < (int)sel.options.size()));
				CHECK(!sel.defined || sel.item == NO_ITEM || sel.item < (int)menu->items.size());
			}
			for (const Item &it : menu->items)
				CHECK(it.image.empty() || ValidPicturePath(it.image));
		}
	}
}

//! Every prefix of a valid message, and the message with every byte changed: no crash, no read past the end
static void TestTruncatedAndMutated()
{
	W full = Costumes();
	full.Op(OP_END).Byte(2).Short(7).Op(OP_OPEN).Byte(2).Short(7).Byte(1).Short(2);
	for (size_t n = 0; n <= full.b.size(); n++)
	{
		Store s;
		std::vector<Event> ev;
		// a heap copy of exactly n bytes, so AddressSanitizer catches any read past it
		unsigned char *copy = (unsigned char *)malloc(n ? n : 1);
		memcpy(copy, full.b.data(), n);
		s.Parse(copy, n, ev);
		free(copy);
	}
	for (size_t i = 0; i < full.b.size(); i++)
	{
		for (int v = 0; v < 256; v += 17)
		{
			Store s;
			std::vector<Event> ev;
			std::vector<unsigned char> m = full.b;
			m[i] = (unsigned char)v;
			unsigned char *copy = (unsigned char *)malloc(m.size());
			memcpy(copy, m.data(), m.size());
			s.Parse(copy, m.size(), ev);
			free(copy);
			for (const Event &e : ev)
			{
				if (e.kind == Event::OPEN)
				{
					const Menu *menu = s.Get(e.a);
					CHECK(menu && (e.c == 0 || e.c < (int)menu->items.size()));
				}
			}
		}
	}
}

static void TestRandom()
{
	Store s;
	for (int round = 0; round < 200000; round++)
	{
		const size_t n = Rand() % (MSG_LIMIT + 1);
		unsigned char *buf = (unsigned char *)malloc(n ? n : 1);
		for (size_t i = 0; i < n; i++)
		{
			// mostly plausible bytes: ops, small numbers, letters, NULs
			const unsigned r = Rand();
			switch (r % 4)
			{
			case 0: buf[i] = (unsigned char)(1 + (r >> 4) % OP_LAST); break;
			case 1: buf[i] = (unsigned char)((r >> 4) % 20); break;
			case 2: buf[i] = (unsigned char)('a' + (r >> 4) % 26); break;
			default: buf[i] = (unsigned char)(r >> 4); break;
			}
		}
		std::vector<Event> ev;
		s.Parse(buf, n, ev, (float)(round % 1000));
		free(buf);
		CHECK(s.TextBytes() <= (size_t)MAX_TEXT_BYTES);
		for (const Event &e : ev)
		{
			if (e.kind == Event::OPEN)
			{
				const Menu *m = s.Get(e.a);
				CHECK(m != nullptr);
				if (m)
				{
					CHECK(e.b == 0 || e.b < (int)m->tabs.size());
					CHECK(e.c == 0 || e.c < (int)m->items.size());
				}
			}
		}
		for (int menu = 0; menu < MAX_SERVER_MENUS; menu++)
		{
			const Menu *m = s.Get(menu);
			if (!m)
				continue;
			CHECK(m->items.size() <= (size_t)MAX_MENU_ITEMS && m->tabs.size() <= (size_t)MAX_TABS);
			for (const Item &it : m->items)
			{
				CHECK(it.tab == 0 || it.tab < (int)m->tabs.size());
				CHECK(it.picture == PICTURE_NONE || it.picture < (int)m->pictures.size());
				for (const Model &mdl : it.models)
					CHECK(!mdl.set || ValidModelPath(m->ModelPath(mdl)));
				CHECK(it.cooldownEnd >= 0.0f && it.cooldownTotal >= 0.0f && it.cooldownTotal <= 6553.6f);
				CHECK(it.cooldownEnd == 0.0f || it.cooldownTotal > 0.0f);
			}
			CHECK(m->fields.size() <= (size_t)MAX_MENU_FIELDS);
			CHECK(m->sliders.size() <= (size_t)MAX_MENU_SLIDERS);
			CHECK(m->toggles.size() <= (size_t)MAX_MENU_TOGGLES);
			CHECK(m->selects.size() <= (size_t)MAX_MENU_SELECTS);
			for (const Select &sel : m->selects)
				CHECK(!sel.defined || (sel.value < (int)sel.options.size() && sel.options.size() <= (size_t)MAX_SELECT_OPTIONS));
			for (const Item &it : m->items)
				CHECK(it.image.empty() || ValidPicturePath(it.image));
			for (const Slider &sl : m->sliders)
				CHECK(sl.min <= sl.max && sl.step >= 1 && sl.value >= sl.min && sl.value <= sl.max);
			for (const Field &f : m->fields)
				CHECK(f.maxLength >= 1 && f.maxLength <= LEN_FIELD_TEXT && f.label.size() <= (size_t)LEN_FIELD_LABEL);
		}
		const HudBar &h = s.Hud();
		CHECK(h.title.size() <= (size_t)LEN_HUD_TITLE && h.sub.size() <= (size_t)LEN_HUD_LINE);
		CHECK(h.next.size() <= (size_t)LEN_HUD_LINE && h.length >= 0.0f && h.length <= 65535.0f);
		if (round % 5000 == 0)
			s.Reset();
	}
}

//! Protocol version 3: item labels up to LEN_LABEL (a help line in a full-width row); a longer one is cut there (on a
//! UTF-8 character); a version 2 server's records (labels of at most LEN_LABEL_V2) parse as before
static void TestLongLabels()
{
	CHECK(PROTOCOL_VERSION >= 3 && PROTOCOL_MIN == 2 && LEN_LABEL == 119 && LEN_LABEL_V2 == 47);
	const std::string help = "Have a map, plugin, or feature idea? Drop it in #syndicate-blackops on discord.sbotnas.io";
	const std::string full(LEN_LABEL, 'L');
	const std::string price(LEN_PRICE, 'P'), badge(LEN_BADGE, 'B'), action(LEN_ACTION, 'A');
	Store s;
	std::vector<Event> ev;
	W def = Define(4, 3, LAYOUT_LIST, MENU_POPUP, 4, 1, 0, 0, 0, "Server Help");
	def.Op(OP_TAB).Byte(4).Byte(0).Str("Common Server Commands");
	W item0;
	item0.Op(OP_ITEM).Byte(4).Short(0).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str(help.c_str()).Str("").Str("")
	    .Str("");
	// every string at its longest: the record still fits one message
	W item1;
	item1.Op(OP_ITEM).Byte(4).Short(1).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str(full.c_str())
	    .Str(price.c_str()).Str(badge.c_str()).Str(action.c_str());
	CHECK(item1.b.size() <= (size_t)MSG_BYTES);
	// past LEN_LABEL: cut to it; a 2-byte character across the limit goes whole
	std::string over = std::string(LEN_LABEL - 1, 'x') + "\xc3\xa9" + "tail";
	W item2;
	item2.Op(OP_ITEM).Byte(4).Short(2).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str(over.c_str()).Str("").Str("")
	    .Str("");
	// a version 2 server's label (the API's cut for a version 2 client: 44 bytes and "...")
	const std::string v2 = help.substr(0, LEN_LABEL_V2 - 3) + "...";
	W item3;
	item3.Op(OP_ITEM).Byte(4).Short(3).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str(v2.c_str()).Str("").Str("")
	    .Str("");
	W end;
	end.Op(OP_END).Byte(4).Short(3);
	CHECK(Feed(s, def) && Feed(s, item0) && Feed(s, item1) && Feed(s, item2) && Feed(s, item3));
	CHECK(Feed(s, end, &ev));
	const Menu *m = s.Get(4);
	CHECK(m != nullptr && m->items.size() == 4);
	if (!m || m->items.size() != 4)
		return;
	CHECK(m->items[0].label == help);
	CHECK(m->items[1].label == full && m->items[1].price == price && m->items[1].action == action);
	CHECK(m->items[2].label == std::string(LEN_LABEL - 1, 'x'));
	CHECK(m->items[3].label == v2 && v2.size() == (size_t)LEN_LABEL_V2);
	// LABEL (live) takes the same length; a record after a long one in the same message still applies
	W live;
	live.Op(OP_LABEL).Byte(4).Short(0).Str((help + " and more text past the limit to see the cut at its end").c_str());
	live.Op(OP_LABEL).Byte(4).Short(3).Str("short");
	CHECK(Feed(s, live, &ev));
	CHECK(s.Get(4)->items[0].label.size() == (size_t)LEN_LABEL);
	CHECK(s.Get(4)->items[3].label == "short");
	// a version 2 server's HELLO: the client reads it (and answers 3; the API takes the smaller)
	W hello2;
	hello2.Op(OP_HELLO).Byte(2);
	CHECK(Feed(s, hello2, &ev) && ev.size() == 1 && ev[0].kind == Event::HELLO && ev[0].a == 2);
}

//! Protocol version 4: LAYOUT_STARS (a star rating) and STARS (the average row); a bad layout reads as a list; STARS
//! past its range is no average, its text is cut, and a short or late one is handled as the other live records
static void TestStars()
{
	CHECK(PROTOCOL_VERSION == 4 && LAYOUT_STARS == 2 && OP_STARS == 37 && OP_LAST == OP_STARS);
	static const char *names[] = { "Terrible", "Bad", "Okay", "Good", "Great" };
	Store s;
	std::vector<Event> ev;
	W w = Define(6, 4, LAYOUT_STARS, MENU_POPUP | MENU_NOCLOSE, 5, 0, 4, 0, 1, "Rate this map");
	for (int i = 0; i < 5; i++)
		w.Op(OP_ITEM).Byte(6).Short(i).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(i == 3 ? ITEM_PICKED : 0).Str(names[i])
		    .Str("").Str("").Str("Rate");
	w.Op(OP_BUTTON).Byte(6).Byte(0).Str("Skip").Str("");
	w.Op(OP_HEADER).Byte(6).Byte(0).Str("co_sbotest");
	w.Op(OP_STARS).Byte(6).Byte(43).Str("4.3 \xc2\xb7 44 ratings");
	w.Op(OP_END).Byte(6).Short(4);
	CHECK(Feed(s, w, &ev));
	const Menu *m = s.Get(6);
	CHECK(m != nullptr);
	if (!m)
		return;
	CHECK(m->layout == LAYOUT_STARS && m->items.size() == 5 && m->items[3].flags == ITEM_PICKED);
	CHECK(m->starsTenths == 43 && m->starsText == "4.3 \xc2\xb7 44 ratings");
	// live: a new average; past 10 x MAX_STARS: none; a long text cut to LEN_STARS; another menu's: dropped
	W live;
	live.Op(OP_STARS).Byte(6).Byte(50).Str("5.0 \xc2\xb7 1 rating");
	live.Op(OP_STARS).Byte(MAX_SERVER_MENUS).Byte(10).Str("past the menus");
	CHECK(Feed(s, live, &ev));
	CHECK(s.Get(6)->starsTenths == 50 && s.Get(6)->starsText == "5.0 \xc2\xb7 1 rating");
	W bad;
	bad.Op(OP_STARS).Byte(6).Byte(10 * MAX_STARS + 1).Str(std::string(80, 's').c_str());
	CHECK(Feed(s, bad, &ev));
	CHECK(s.Get(6)->starsTenths == STARS_NONE && s.Get(6)->starsText.size() == (size_t)LEN_STARS);
	W none;
	none.Op(OP_STARS).Byte(6).Byte(STARS_NONE).Str("No ratings yet");
	none.Op(OP_STARS).Byte(6); // short: dropped, the message ends there
	CHECK(!Feed(s, none, &ev));
	CHECK(s.Get(6)->starsTenths == STARS_NONE && s.Get(6)->starsText == "No ratings yet");
	int changed = 0;
	for (const Event &e : ev)
		changed += e.kind == Event::CHANGED && e.a == 6;
	CHECK(changed == 1); // the last message: one STARS applied before the short one
	// a layout this client does not know: a list
	W odd = Define(7, 1, 9, 0, 1, 0, 0, 0, 0, "Odd");
	odd.Op(OP_ITEM).Byte(7).Short(0).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str("a").Str("").Str("").Str("");
	odd.Op(OP_END).Byte(7).Short(1);
	CHECK(Feed(s, odd));
	CHECK(s.Get(7) && s.Get(7)->layout == LAYOUT_LIST);
	// a new definition starts without an average
	W again = Define(6, 5, LAYOUT_STARS, 0, 5, 0, 0, 0, 0, "Rate this map");
	for (int i = 0; i < 5; i++)
		again.Op(OP_ITEM).Byte(6).Short(i).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str(names[i]).Str("").Str("")
		    .Str("");
	again.Op(OP_END).Byte(6).Short(5);
	CHECK(Feed(s, again));
	CHECK(s.Get(6)->starsTenths == STARS_NONE && s.Get(6)->starsText.empty());
}

//! The Pawn include's constants (server_menu.inc: "const SMENU_X = n" / "#define SMENU_X n") equal this header's
//! Menu ids: every id below MAX_SERVER_MENUS (32) is a menu, 16..31 too; MAX_SERVER_MENUS and past it are refused
//! or dropped, never stored
static void TestMenuIds()
{
	static_assert(MAX_SERVER_MENUS == 32, "the protocol's menu ids are 0..31");
	static_assert(MAX_SERVER_MENUS < CLOSE_ANY, "a menu id is a byte and CLOSE_ANY is not a menu");
	Store s;
	std::vector<Event> ev;
	for (int menu : { 15, 16, 31 })
	{
		W w = Define(menu, 100 + menu, LAYOUT_LIST, 0, 1, 0, 0, 0, 0, "ids");
		w.Op(OP_ITEM).Byte(menu).Short(0).Byte(0).Byte(PICTURE_NONE).Byte(0).Byte(0).Str("one").Str("").Str("")
		    .Str("Pick");
		w.Op(OP_END).Byte(menu).Short(100 + menu);
		w.Op(OP_OPEN).Byte(menu).Short(100 + menu).Byte(0).Short(0);
		CHECK(Feed(s, w, &ev));
		const Menu *m = s.Get(menu);
		CHECK(m != nullptr && m->serial == 100 + menu && m->items.size() == 1 && m->items[0].label == "one");
		bool opened = false;
		for (const Event &e : ev)
			opened |= e.kind == Event::OPEN && e.a == menu;
		CHECK(opened);
	}
	// live records for the last menu apply; for MAX_SERVER_MENUS (32) and 254 they are dropped alone
	W live;
	live.Op(OP_LABEL).Byte(31).Short(0).Str("renamed");
	live.Op(OP_LABEL).Byte(MAX_SERVER_MENUS).Short(0).Str("past the menus");
	live.Op(OP_STARS).Byte(254).Byte(10).Str("past the menus");
	live.Op(OP_OPEN).Byte(MAX_SERVER_MENUS).Short(1).Byte(0).Short(0);
	live.Op(OP_END).Byte(MAX_SERVER_MENUS).Short(1);
	CHECK(Feed(s, live, &ev));
	CHECK(s.Get(31) && s.Get(31)->items[0].label == "renamed");
	for (const Event &e : ev)
		CHECK(e.a < MAX_SERVER_MENUS);
	CHECK(!s.Get(MAX_SERVER_MENUS) && !s.Get(254) && !s.Get(-1));
	// a definition for 32 or 254 is refused (the rest of that message dropped); the stored menus stay
	CHECK(!Feed(s, Define(MAX_SERVER_MENUS, 1, 0, 0, 1, 0, 0, 0, 0, "x")));
	CHECK(!Feed(s, Define(254, 1, 0, 0, 1, 0, 0, 0, 0, "x")));
	CHECK(s.Get(15) && s.Get(16) && s.Get(31));
	// CLOSE_ANY still closes, a close of 32 is dropped
	W close;
	close.Op(OP_CLOSE).Byte(MAX_SERVER_MENUS).Op(OP_CLOSE).Byte(CLOSE_ANY);
	CHECK(Feed(s, close, &ev));
	CHECK(ev.size() == 1 && ev[0].kind == Event::CLOSE && ev[0].a == CLOSE_ANY);
}

static void TestInclude(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
	{
		fprintf(stderr, "cannot read %s\n", path);
		s_iFailures++;
		return;
	}
	std::map<std::string, long> values;
	char line[512];
	while (fgets(line, sizeof(line), f))
	{
		char name[64];
		long v;
		if (sscanf(line, " #define SMENU_%63[A-Z_0-9] %li", name, &v) == 2 ||
		    sscanf(line, " const SMENU_%63[A-Z_0-9] = %li", name, &v) == 2)
			values[name] = v;
	}
	fclose(f);
	const struct { const char *name; long value; } expected[] = {
		{ "PROTOCOL", PROTOCOL_VERSION }, { "LIST", LAYOUT_LIST }, { "GRID", LAYOUT_GRID },
		{ "SIDE", MENU_SIDE }, { "SEARCH", MENU_SEARCH }, { "BACK", MENU_BACK }, { "NOCLOSE", MENU_NOCLOSE },
		{ "PREVIEW", MENU_PREVIEW }, { "POPUP", MENU_POPUP }, { "LARGE", MENU_LARGE }, { "ALLTAB", MENU_ALLTAB },
		{ "METER_NONE", METER_NONE }, { "MAX_BADGES", MAX_BADGES }, { "MAX_VOTERS", MAX_VOTERS },
		{ "BADGE_PLAIN", BADGE_PLAIN }, { "BADGE_GREEN", BADGE_GREEN }, { "BADGE_GOLD", BADGE_GOLD }, { "BADGE_DIM", BADGE_DIM }, { "DISABLED", ITEM_DISABLED }, { "PICKED", ITEM_PICKED },
		{ "HIDDEN", ITEM_HIDDEN }, { "MAX_MENUS", MAX_SERVER_MENUS }, { "MAX_ITEMS", MAX_MENU_ITEMS }, { "MAX_TABS", MAX_TABS },
		{ "MAX_HEADERS", MAX_HEADERS }, { "MAX_PICTURES", MAX_PICTURES }, { "MAX_BUTTONS", MAX_MENU_BUTTONS },
		{ "LAYER_PLAYER", 0 }, { "LAYERS", MODEL_LAYERS }, { "MAX_MODEL_PATHS", MAX_MODEL_PATHS },
		{ "MAX_FIELDS", MAX_MENU_FIELDS }, { "TEXT_MAX", LEN_FIELD_TEXT }, { "MAX_SLIDERS", MAX_MENU_SLIDERS },
		{ "MAX_TOGGLES", MAX_MENU_TOGGLES }, { "MAX_SELECTS", MAX_MENU_SELECTS },
		{ "MAX_OPTIONS", MAX_SELECT_OPTIONS }, { "CONFIRM_ITEM", CONFIRM_ITEM }, { "CONFIRM_BUTTON", CONFIRM_BUTTON },
		{ "BUTTON_DISABLED", BUTTON_DISABLED }, { "LABEL_MAX", LEN_LABEL }, { "LABEL_MAX_V2", LEN_LABEL_V2 },
		{ "STARS", LAYOUT_STARS }, { "STARS_NONE", STARS_NONE }, { "MAX_STARS", MAX_STARS }, { "STARS_TEXT_MAX", LEN_STARS },
	};
	for (const auto &e : expected)
	{
		auto found = values.find(e.name);
		const bool ok = found != values.end() && found->second == e.value;
		if (!ok)
			fprintf(stderr, "server_menu.inc: SMENU_%s %s (header: %ld)\n", e.name,
			        found == values.end() ? "missing" : "differs", e.value);
		CHECK(ok);
	}
}

int main(int argc, char **argv)
{
	TestRoundTrip();
	TestLongLabels();
	TestStars();
	TestMenuIds();
	TestHostile();
	TestPaths();
	TestHudAndText();
	TestSelectConfirmPicture();
	TestTruncatedAndMutated();
	TestRandom();
	if (argc > 1)
		TestInclude(argv[1]);
	printf("server_menu: %d checks, %d failed\n", s_iChecks, s_iFailures);
	return s_iFailures ? 1 : 0;
}
