//========= SBO 2.0: server-driven menus, client store =========//
//
// Purpose: see sbo_server_menu_proto.h.
//=================================================================//

#include "sbo_server_menu_proto.h"

#include <stdio.h>
#include <string.h>

namespace servermenu
{
//-----------------------------------------------------------------------------
// Bounded reader of one message (the engine's READ_* layout: little-endian shorts, NUL-terminated strings)
//-----------------------------------------------------------------------------
class Reader
{
public:
	Reader(const unsigned char *data, size_t size) : m_pData(data), m_Size(size) {}

	bool Bad() const { return m_bBad; }
	bool AtEnd() const { return m_Pos >= m_Size; }

	//! The next len bytes as their own reader (a record's body); false when the message has fewer left
	bool Take(size_t len, Reader &body)
	{
		if (m_bBad || len > m_Size - m_Pos)
		{
			m_bBad = true;
			m_Pos = m_Size;
			return false;
		}
		body = Reader(m_pData + m_Pos, len);
		m_Pos += len;
		return true;
	}

	int Byte()
	{
		if (m_Pos >= m_Size)
		{
			m_bBad = true;
			return 0;
		}
		return m_pData[m_Pos++];
	}

	int Short()
	{
		const int lo = Byte();
		const int hi = Byte();
		return lo | (hi << 8);
	}

	//! Two little-endian longs: a 64-bit number (a SteamID64), low half first
	unsigned long long Long64()
	{
		unsigned long long v = 0;
		for (int i = 0; i < 8; i++)
			v |= (unsigned long long)Byte() << (8 * i);
		return v;
	}

	//! A string cut to max bytes; a string without its NUL inside the message is bad
	std::string String(size_t max, bool keepNewlines = false)
	{
		if (m_bBad || m_Pos >= m_Size)
		{
			m_bBad = true;
			return std::string();
		}
		const unsigned char *start = m_pData + m_Pos;
		const void *nul = memchr(start, 0, m_Size - m_Pos);
		if (!nul)
		{
			m_bBad = true;
			m_Pos = m_Size;
			return std::string();
		}
		const size_t len = (const unsigned char *)nul - start;
		m_Pos += len + 1;
		return CleanString((const char *)start, len, max, keepNewlines);
	}

private:
	const unsigned char *m_pData;
	size_t m_Size;
	size_t m_Pos = 0;
	bool m_bBad = false;
};

std::string CleanString(const char *s, size_t len, size_t max, bool keepNewlines)
{
	std::string out;
	out.reserve(len < max ? len : max);
	for (size_t i = 0; i < len; i++)
	{
		const unsigned char c = (unsigned char)s[i];
		if ((c < 0x20 && !(keepNewlines && c == '\n')) || c == 0x7F)
			continue;
		out.push_back((char)c);
	}
	if (out.size() > max)
	{
		size_t cut = max;
		// not inside a UTF-8 character: back to its lead byte
		while (cut > 0 && ((unsigned char)out[cut] & 0xC0) == 0x80)
			cut--;
		out.resize(cut);
	}
	return out;
}

std::string CleanFieldText(const std::string &s, size_t max)
{
	std::string out;
	for (char ch : s)
	{
		const unsigned char c = (unsigned char)ch;
		if (c < 0x20 || c == 0x7F || c == '"' || c == ';' || c == '\\')
			continue;
		out.push_back(ch);
	}
	const size_t first = out.find_first_not_of(' ');
	if (first == std::string::npos)
		return std::string();
	out = out.substr(first, out.find_last_not_of(' ') - first + 1);
	return CleanString(out.c_str(), out.size(), max);
}

namespace
{
bool ValidPath(const std::string &path, const char *const *prefixes, const char *ext, bool upper)
{
	if (path.empty() || path.size() > (size_t)LEN_PATH)
		return false;
	bool prefixOk = false;
	for (const char *const *p = prefixes; *p; p++)
	{
		if (path.compare(0, strlen(*p), *p) == 0)
			prefixOk = true;
	}
	if (!prefixOk)
		return false;
	int dots = 0;
	char prev = 0;
	for (char c : path)
	{
		const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '/' ||
		                c == '.' || (upper && c >= 'A' && c <= 'Z');
		if (!ok)
			return false;
		if (c == '.')
			dots++;
		if (c == '/' && prev == '/')
			return false;
		prev = c;
	}
	const size_t e = strlen(ext);
	return dots == 1 && path.size() > e && path.compare(path.size() - e, e, ext) == 0 &&
	       path[path.size() - e - 1] != '/';
}
} // namespace

bool ValidPicturePath(const std::string &path)
{
	static const char *const prefixes[] = { "gfx/", "sprites/", nullptr };
	return ValidPath(path, prefixes, ".tga", false);
}

bool ValidMapName(const std::string &map)
{
	if (map.empty() || map.size() > (size_t)LEN_MAP || map.find("..") != std::string::npos)
		return false;
	for (char c : map)
	{
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
			return false;
	}
	return true;
}

bool ValidModelPath(const std::string &path)
{
	// player model folders keep the case their authors gave them (models/player/<name>/<name>.mdl)
	static const char *const prefixes[] = { "models/", nullptr };
	return ValidPath(path, prefixes, ".mdl", true);
}

//-----------------------------------------------------------------------------
// Menu
//-----------------------------------------------------------------------------
size_t Menu::TextBytes() const
{
	size_t n = title.size();
	for (const std::string &s : tabs)
		n += s.size();
	for (const Picture &p : pictures)
		n += p.path.size();
	for (const std::string &s : buttons)
		n += s.size();
	for (const std::string &s : buttonTips)
		n += s.size();
	for (const Field &f : fields)
		n += f.label.size() + f.hint.size() + f.button.size();
	for (const Slider &sl : sliders)
		n += sl.label.size() + sl.tip.size();
	for (const Toggle &t : toggles)
		n += t.label.size() + t.tip.size();
	for (const std::string &s : modelPaths)
		n += s.size();
	for (const std::string &s : headers)
		n += s.size();
	n += starsText.size();
	for (const Item &it : items)
		n += it.label.size() + it.price.size() + it.badge.size() + it.action.size() + it.text.size() + it.map.size() + it.line.size() + it.title.size() +
		     it.image.size() + it.confirm.size();
	for (const Select &sel : selects)
	{
		n += sel.label.size() + sel.tip.size();
		for (const std::string &o : sel.options)
			n += o.size();
	}
	for (const std::string &s : buttonConfirms)
		n += s.size();
	for (const Item &it : items)
		for (const Badge &b : it.badges)
			n += b.text.size() + b.tip.size();
	return n;
}

const std::string &Menu::ModelPath(const Model &m) const
{
	static const std::string none;
	return m.set && m.path >= 0 && m.path < (int)modelPaths.size() ? modelPaths[m.path] : none;
}

//-----------------------------------------------------------------------------
// Store
//-----------------------------------------------------------------------------
void Store::Reset()
{
	for (int i = 0; i < MAX_SERVER_MENUS; i++)
	{
		m_Live[i] = Menu();
		m_Pending[i] = Menu();
		m_bPending[i] = false;
	}
	m_Hud = HudBar();
	m_LastError.clear();
	m_bErrorReported = false;
	m_bErrorNew = false;
}

const Menu *Store::Get(int menu) const
{
	if (menu < 0 || menu >= MAX_SERVER_MENUS || !m_Live[menu].complete)
		return nullptr;
	return &m_Live[menu];
}

size_t Store::TextBytes() const
{
	size_t n = 0;
	for (int i = 0; i < MAX_SERVER_MENUS; i++)
	{
		n += m_Live[i].TextBytes();
		if (m_bPending[i])
			n += m_Pending[i].TextBytes();
	}
	return n;
}

bool Store::Fits(size_t more) const
{
	return TextBytes() + more <= (size_t)MAX_TEXT_BYTES;
}

bool Store::TakeErrorToReport()
{
	if (!m_bErrorNew || m_bErrorReported)
		return false;
	m_bErrorNew = false;
	m_bErrorReported = true;
	return true;
}

void Store::Fail(const char *why, int menu)
{
	if (menu >= 0 && menu < MAX_SERVER_MENUS && m_bPending[menu])
	{
		m_bPending[menu] = false;
		m_Pending[menu] = Menu();
	}
	if (m_LastError.empty())
	{
		char text[160];
		snprintf(text, sizeof(text), "ServerMenu: %s (menu %d); the rest of the message was dropped\n", why, menu);
		m_LastError = text;
		m_bErrorNew = true;
	}
}

Menu *Store::Target(int menu)
{
	if (menu < 0 || menu >= MAX_SERVER_MENUS)
		return nullptr;
	if (m_bPending[menu])
		return &m_Pending[menu];
	return m_Live[menu].complete ? &m_Live[menu] : nullptr;
}

bool Store::Parse(const unsigned char *data, size_t size, std::vector<Event> &events, float now)
{
	if (!data)
		return size == 0;
	Reader r(data, size);
	bool ok = true;
	while (!r.AtEnd())
	{
		// every record: byte op, byte length, then that many bytes
		const int op = r.Byte();
		const int len = r.Byte();
		Reader body(nullptr, 0);
		if (r.Bad() || !r.Take((size_t)len, body))
		{
			Fail("a record past the message's end", -1);
			return false; // its length is wrong: where the next record starts is unknown
		}
		if (op < 1 || op > OP_LAST)
			continue; // a newer server's record: skipped
		// a bad record is dropped (its pending definition too); the next one starts at its length
		if (!ParseRecord(op, body, events, now))
			ok = false;
	}
	return ok;
}

bool Store::ParseRecord(int op, Reader &r, std::vector<Event> &events, float now)
{
	// a newer server may append fields to a record: the bytes past the ones read here are skipped
	switch (op)
	{
	case OP_HELLO:
	{
		const int version = r.Byte();
		if (r.Bad())
		{
			Fail("short HELLO", -1);
			return false;
		}
		events.push_back({ Event::HELLO, version, 0, 0 });
		return true;
	}
	case OP_DEFINE:
	{
		const int menu = r.Byte();
		const int serial = r.Short();
		const int layout = r.Byte();
		const int flags = r.Byte();
		const int items = r.Short();
		const int tabs = r.Byte();
		const int headers = r.Byte();
		const int pictures = r.Byte();
		const int buttons = r.Byte();
		const int modelPaths = r.Byte();
		const int fields = r.Byte();
		const int sliders = r.Byte();
		const int toggles = r.Byte();
		std::string title = r.String(LEN_TITLE);
		if (r.Bad())
		{
			Fail("short DEFINE", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS || items > MAX_MENU_ITEMS || tabs > MAX_TABS || headers > MAX_HEADERS ||
		    pictures > MAX_PICTURES || buttons > MAX_MENU_BUTTONS || modelPaths > MAX_MODEL_PATHS ||
		    fields > MAX_MENU_FIELDS || sliders > MAX_MENU_SLIDERS ||
		    toggles > MAX_MENU_TOGGLES)
		{
			Fail("DEFINE over the caps", menu < MAX_SERVER_MENUS ? menu : -1);
			return false;
		}
		// a new definition replaces a pending one
		m_bPending[menu] = false;
		m_Pending[menu] = Menu();
		if (!Fits(title.size()))
		{
			Fail("text limit", menu);
			return false;
		}
		Menu &m = m_Pending[menu];
		m.serial = serial;
		m.layout = (layout == LAYOUT_GRID || layout == LAYOUT_STARS) ? layout : LAYOUT_LIST;
		m.flags = flags & MENU_FLAGS_ALL;
		m.title = title;
		m.items.resize(items);
		m.tabs.resize(tabs);
		m.pictures.resize(pictures);
		m.buttons.resize(buttons);
		m.buttonTips.resize(buttons);
		m.buttonFlags.assign(buttons, 0);
		m.buttonConfirms.resize(buttons);
		m.modelPaths.resize(modelPaths);
		m.fields.resize(fields);
		m.sliders.resize(sliders);
		m.toggles.resize(toggles);
		(void)headers; // the four lines always exist; the count says how many the server fills
		m_bPending[menu] = true;
		return true;
	}
	case OP_TAB:
	case OP_BUTTON:
	{
		const int menu = r.Byte();
		const int index = r.Byte();
		std::string label = r.String(op == OP_TAB ? LEN_TAB : LEN_BUTTON);
		std::string tip = op == OP_BUTTON ? r.String(LEN_CONTROL_TIP) : std::string();
		if (r.Bad())
		{
			Fail(op == OP_TAB ? "short TAB" : "short BUTTON", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS || !m_bPending[menu])
			return true; // not being defined: dropped
		std::vector<std::string> &list = op == OP_TAB ? m_Pending[menu].tabs : m_Pending[menu].buttons;
		if (index >= (int)list.size())
			return true;
		if (!Fits(label.size() + tip.size()))
		{
			Fail("text limit", menu);
			return false;
		}
		list[index] = label;
		if (op == OP_BUTTON && index < (int)m_Pending[menu].buttonTips.size())
			m_Pending[menu].buttonTips[index] = tip;
		return true;
	}
	case OP_PICTURE:
	{
		const int menu = r.Byte();
		const int index = r.Byte();
		const int columns = r.Byte();
		const int rows = r.Byte();
		std::string path = r.String(LEN_PATH);
		if (r.Bad())
		{
			Fail("short PICTURE", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS || !m_bPending[menu] || index >= (int)m_Pending[menu].pictures.size())
			return true;
		if (!ValidPicturePath(path))
			path.clear(); // drawn as "no picture"
		if (!Fits(path.size()))
		{
			Fail("text limit", menu);
			return false;
		}
		Picture &p = m_Pending[menu].pictures[index];
		p.path = path;
		p.columns = columns < 1 ? 1 : (columns > MAX_SHEET_CELLS ? MAX_SHEET_CELLS : columns);
		p.rows = rows < 1 ? 1 : (rows > MAX_SHEET_CELLS ? MAX_SHEET_CELLS : rows);
		return true;
	}
	case OP_ITEM:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		const int tab = r.Byte();
		const int picture = r.Byte();
		const int cell = r.Byte();
		const int flags = r.Byte();
		std::string label = r.String(LEN_LABEL);
		std::string price = r.String(LEN_PRICE);
		std::string badge = r.String(LEN_BADGE);
		std::string action = r.String(LEN_ACTION);
		if (r.Bad())
		{
			Fail("short ITEM", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS || !m_bPending[menu])
			return true;
		Menu &m = m_Pending[menu];
		if (index >= (int)m.items.size())
			return true;
		Item &it = m.items[index];
		const size_t oldBytes = it.label.size() + it.price.size() + it.badge.size() + it.action.size();
		const size_t newBytes = label.size() + price.size() + badge.size() + action.size();
		if (newBytes > oldBytes && !Fits(newBytes - oldBytes))
		{
			Fail("text limit", menu);
			return false;
		}
		it.defined = true;
		it.tab = tab < (int)m.tabs.size() ? tab : 0;
		it.picture = picture < (int)m.pictures.size() ? picture : PICTURE_NONE;
		it.cell = 0;
		if (it.picture != PICTURE_NONE)
		{
			const Picture &p = m.pictures[it.picture];
			it.cell = cell < p.columns * p.rows ? cell : 0;
		}
		it.flags = flags & ITEM_FLAGS_ALL;
		it.label = label;
		it.price = price;
		it.badge = badge;
		it.action = action;
		return true;
	}
	case OP_ITEMTEXT:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		std::string text = r.String(LEN_TEXT, true); // its line breaks kept: lines one under another
		if (r.Bad())
		{
			Fail("short ITEMTEXT", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		// in a definition, and live like LABEL (a queue row's text and its links change with its track)
		if (m_bPending[menu] && index < (int)m_Pending[menu].items.size())
		{
			Item &it = m_Pending[menu].items[index];
			if (text.size() > it.text.size() && !Fits(text.size() - it.text.size()))
			{
				Fail("text limit", menu);
				return false;
			}
			it.text = text;
		}
		if (m_Live[menu].complete && index < (int)m_Live[menu].items.size() && m_Live[menu].items[index].defined)
		{
			Item &it = m_Live[menu].items[index];
			if (text.size() <= it.text.size() || Fits(text.size() - it.text.size()))
			{
				it.text = text;
				events.push_back({ Event::CHANGED, menu, 0, 0 });
			}
		}
		return true;
	}
	case OP_HEADER:
	{
		const int menu = r.Byte();
		const int line = r.Byte();
		std::string text = r.String(LEN_HEADER);
		if (r.Bad())
		{
			Fail("short HEADER", menu);
			return false;
		}
		Menu *m = Target(menu);
		if (!m || line >= MAX_HEADERS)
			return true;
		if (text.size() > m->headers[line].size() && !Fits(text.size() - m->headers[line].size()))
			return true; // a live line over the limit is dropped; the definition stays
		m->headers[line] = text;
		if (m == &m_Live[menu])
			events.push_back({ Event::CHANGED, menu, 0, 0 });
		return true;
	}
	case OP_END:
	{
		const int menu = r.Byte();
		const int serial = r.Short();
		if (r.Bad())
		{
			Fail("short END", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS || !m_bPending[menu])
			return true;
		if (m_Pending[menu].serial != serial)
		{
			Fail("END of another definition", menu);
			return true; // the pending menu is dropped; later records are fine
		}
		m_Live[menu] = m_Pending[menu];
		m_Live[menu].complete = true;
		m_Pending[menu] = Menu();
		m_bPending[menu] = false;
		events.push_back({ Event::DEFINED, menu, 0, 0 });
		return true;
	}
	case OP_OPEN:
	{
		const int menu = r.Byte();
		const int serial = r.Short();
		const int tab = r.Byte();
		const int item = r.Short();
		if (r.Bad())
		{
			Fail("short OPEN", menu);
			return false;
		}
		const Menu *m = Get(menu);
		if (!m || m->serial != serial)
			return true; // not defined (yet), or an older definition: ignored
		events.push_back({ Event::OPEN, menu, tab < (int)m->tabs.size() ? tab : 0,
		                   item < (int)m->items.size() ? item : 0 });
		return true;
	}
	case OP_STATE:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		const int flags = r.Byte();
		std::string price = r.String(LEN_PRICE);
		std::string badge = r.String(LEN_BADGE);
		std::string action = r.String(LEN_ACTION);
		if (r.Bad())
		{
			Fail("short STATE", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->items.size() || !m->items[index].defined)
				continue;
			Item &it = m->items[index];
			const size_t oldBytes = it.price.size() + it.badge.size() + it.action.size();
			const size_t newBytes = price.size() + badge.size() + action.size();
			if (newBytes > oldBytes && !Fits(newBytes - oldBytes))
				continue;
			it.flags = flags & ITEM_FLAGS_ALL;
			it.price = price;
			it.badge = badge;
			it.action = action;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_CLOSE:
	{
		const int menu = r.Byte();
		if (r.Bad())
		{
			Fail("short CLOSE", -1);
			return false;
		}
		if (menu < MAX_SERVER_MENUS || menu == CLOSE_ANY)
			events.push_back({ Event::CLOSE, menu, 0, 0 });
		return true;
	}
	case OP_MODELPATH:
	{
		const int menu = r.Byte();
		const int index = r.Byte();
		std::string path = r.String(LEN_PATH);
		if (r.Bad())
		{
			Fail("short MODELPATH", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS || !m_bPending[menu] || index >= (int)m_Pending[menu].modelPaths.size())
			return true;
		if (!ValidModelPath(path))
			path.clear(); // the layers naming it stay empty
		std::string &slot = m_Pending[menu].modelPaths[index];
		if (path.size() > slot.size() && !Fits(path.size() - slot.size()))
		{
			Fail("text limit", menu);
			return false;
		}
		slot = path;
		return true;
	}
	case OP_MODEL:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		const int layer = r.Byte();
		const int path = r.Byte();
		const int body = r.Short();
		const int skin = r.Byte();
		const int sequence = r.Short();
		const int body1 = r.Short();
		const int sequence1 = r.Short();
		if (r.Bad())
		{
			Fail("short MODEL", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS || !m_bPending[menu] || index >= (int)m_Pending[menu].items.size() ||
		    layer >= MODEL_LAYERS)
			return true;
		const Menu &m = m_Pending[menu];
		Model &mdl = m_Pending[menu].items[index].models[layer];
		mdl = Model();
		// its file first (MODELPATH); a number past them or a path the client refused: an empty preview layer
		if (path >= (int)m.modelPaths.size() || m.modelPaths[path].empty())
			return true;
		mdl.set = true;
		mdl.path = path;
		mdl.body = body;
		mdl.skin = skin;
		mdl.sequence = sequence;
		mdl.body1 = body1 == MODEL_SAME ? body : body1;
		mdl.sequence1 = sequence1 == MODEL_SAME ? sequence : sequence1;
		return true;
	}
	case OP_LABEL:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		std::string label = r.String(LEN_LABEL);
		if (r.Bad())
		{
			Fail("short LABEL", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->items.size() || !m->items[index].defined)
				continue;
			Item &it = m->items[index];
			if (label.size() > it.label.size() && !Fits(label.size() - it.label.size()))
				continue; // a live line over the limit is dropped; the definition stays
			it.label = label;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_COOLDOWN:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		const int left = r.Short();
		const int total = r.Short();
		if (r.Bad())
		{
			Fail("short COOLDOWN", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->items.size() || !m->items[index].defined)
				continue;
			Item &it = m->items[index];
			it.cooldownEnd = left > 0 ? now + left * 0.1f : 0.0f;
			it.cooldownTotal = left > 0 ? (total > left ? total : left) * 0.1f : 0.0f;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_FIELD:
	{
		const int menu = r.Byte();
		const int index = r.Byte();
		const int maxLength = r.Byte();
		std::string label = r.String(LEN_FIELD_LABEL);
		std::string hint = r.String(LEN_FIELD_HINT);
		std::string button = r.String(LEN_BUTTON);
		if (r.Bad())
		{
			Fail("short FIELD", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS || !m_bPending[menu] || index >= (int)m_Pending[menu].fields.size())
			return true;
		Field &f = m_Pending[menu].fields[index];
		const size_t oldBytes = f.label.size() + f.hint.size() + f.button.size();
		const size_t newBytes = label.size() + hint.size() + button.size();
		if (newBytes > oldBytes && !Fits(newBytes - oldBytes))
		{
			Fail("text limit", menu);
			return false;
		}
		f.label = label;
		f.hint = hint;
		f.button = button;
		f.maxLength = maxLength < 1 ? 1 : (maxLength > LEN_FIELD_TEXT ? LEN_FIELD_TEXT : maxLength);
		return true;
	}
	case OP_SLIDER:
	{
		const int menu = r.Byte();
		const int index = r.Byte();
		const int lo = r.Short();
		const int hi = r.Short();
		const int step = r.Short();
		const int value = r.Short();
		std::string label = r.String(LEN_SLIDER_LABEL);
		std::string tip = r.String(LEN_CONTROL_TIP);
		if (r.Bad())
		{
			Fail("short SLIDER", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS || !m_bPending[menu] || index >= (int)m_Pending[menu].sliders.size())
			return true;
		Slider &sl = m_Pending[menu].sliders[index];
		if (label.size() + tip.size() > sl.label.size() + sl.tip.size() &&
		    !Fits(label.size() + tip.size() - sl.label.size() - sl.tip.size()))
		{
			Fail("text limit", menu);
			return false;
		}
		sl.label = label;
		sl.tip = tip;
		sl.min = lo;
		sl.max = hi < lo ? lo : hi;
		sl.step = step < 1 ? 1 : step;
		sl.value = sl.Clamp(value);
		return true;
	}
	case OP_SLIDERVALUE:
	{
		const int menu = r.Byte();
		const int index = r.Byte();
		const int value = r.Short();
		if (r.Bad())
		{
			Fail("short SLIDERVALUE", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->sliders.size())
				continue;
			m->sliders[index].value = m->sliders[index].Clamp(value);
			m->sliders[index].updates++;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_TOGGLE:
	{
		const int menu = r.Byte();
		const int index = r.Byte();
		const int on = r.Byte();
		std::string label = r.String(LEN_TOGGLE_LABEL);
		std::string tip = r.String(LEN_CONTROL_TIP);
		if (r.Bad())
		{
			Fail("short TOGGLE", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS || !m_bPending[menu] || index >= (int)m_Pending[menu].toggles.size())
			return true;
		Toggle &t = m_Pending[menu].toggles[index];
		if (label.size() + tip.size() > t.label.size() + t.tip.size() &&
		    !Fits(label.size() + tip.size() - t.label.size() - t.tip.size()))
		{
			Fail("text limit", menu);
			return false;
		}
		t.label = label;
		t.tip = tip;
		t.on = on != 0;
		return true;
	}
	case OP_TOGGLEVALUE:
	{
		const int menu = r.Byte();
		const int index = r.Byte();
		const int on = r.Byte();
		if (r.Bad())
		{
			Fail("short TOGGLEVALUE", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->toggles.size())
				continue;
			m->toggles[index].on = on != 0;
			m->toggles[index].updates++;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_AVATAR:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		const unsigned long long avatar = r.Long64();
		if (r.Bad())
		{
			Fail("short AVATAR", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->items.size() || !m->items[index].defined)
				continue;
			m->items[index].avatar = avatar;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_MAPPICTURE:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		std::string map = r.String(LEN_MAP);
		if (r.Bad())
		{
			Fail("short MAPPICTURE", menu);
			return false;
		}
		for (char &c : map)
			c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; // the picture files are named in lower case
		if (!map.empty() && !ValidMapName(map))
			map.clear(); // drawn as no picture
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->items.size() || !m->items[index].defined)
				continue;
			std::string &have = m->items[index].map;
			if (map.size() > have.size() && !Fits(map.size() - have.size()))
			{
				if (m == &m_Live[menu])
					continue; // a live change over the limit is dropped; the menu and the message stay
				Fail("text limit", menu);
				return false;
			}
			have = map;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_METER:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		const int percent = r.Byte();
		if (r.Bad())
		{
			Fail("short METER", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->items.size() || !m->items[index].defined)
				continue;
			m->items[index].meter = percent <= 100 ? percent : METER_NONE;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_BADGES:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		const int count = r.Byte();
		std::vector<Badge> badges;
		for (int b = 0; b < count && b < MAX_BADGES && !r.Bad(); b++)
		{
			Badge badge;
			const int style = r.Byte();
			badge.style = style < BADGE_STYLES ? style : BADGE_PLAIN;
			badge.text = r.String(LEN_BADGE);
			badge.tip = r.String(LEN_BADGE_TIP);
			badges.push_back(badge);
		}
		if (r.Bad() || count > MAX_BADGES)
		{
			Fail("bad BADGES", menu);
			return false;
		}
		size_t bytes = 0;
		for (const Badge &b : badges)
			bytes += b.text.size() + b.tip.size();
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->items.size() || !m->items[index].defined)
				continue;
			size_t have = 0;
			for (const Badge &b : m->items[index].badges)
				have += b.text.size() + b.tip.size();
			if (bytes > have && !Fits(bytes - have))
			{
				Fail("text limit", menu);
				return false;
			}
			m->items[index].badges = badges;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_VOTERS:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		const int count = r.Byte();
		std::vector<int> voters;
		for (int v = 0; v < count && v < MAX_VOTERS && !r.Bad(); v++)
		{
			const int client = r.Byte();
			if (client >= 1 && client <= MAX_VOTERS)
				voters.push_back(client);
		}
		if (r.Bad() || count > MAX_VOTERS)
		{
			Fail("bad VOTERS", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->items.size() || !m->items[index].defined)
				continue;
			m->items[index].voters = voters;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_ITEMTITLE:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		std::string title = r.String(LEN_ITEMTITLE);
		if (r.Bad())
		{
			Fail("short ITEMTITLE", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->items.size() || !m->items[index].defined)
				continue;
			std::string &have = m->items[index].title;
			if (title.size() > have.size() && !Fits(title.size() - have.size()))
				continue; // over the text limit: dropped, the label stays the title
			have = title;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_ITEMLINE:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		std::string line = r.String(LEN_ITEMLINE);
		if (r.Bad())
		{
			Fail("short ITEMLINE", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->items.size() || !m->items[index].defined)
				continue;
			std::string &have = m->items[index].line;
			if (line.size() > have.size() && !Fits(line.size() - have.size()))
			{
				if (m == &m_Live[menu])
					continue; // a live change over the limit is dropped; the menu and the message stay
				Fail("text limit", menu);
				return false;
			}
			have = line;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_HUDBAR:
	{
		const int flags = r.Byte();
		const int played = r.Short();
		const int length = r.Short();
		const int nextSeconds = r.Short();
		const unsigned long long avatar = r.Long64();
		const unsigned long long nextAvatar = r.Long64();
		std::string title = r.String(LEN_HUD_TITLE);
		std::string sub = r.String(LEN_HUD_LINE);
		std::string next = r.String(LEN_HUD_LINE);
		if (r.Bad())
		{
			Fail("short HUDBAR", -1);
			return false;
		}
		// one strip, its own bounded strings (not in the menus' text limit)
		HudBar h;
		h.shown = (flags & HUDBAR_SHOW) != 0;
		if (h.shown)
		{
			h.title = title;
			h.sub = sub;
			h.next = next;
			h.length = (float)length;
			h.start = now - (float)(played < length || !length ? played : length);
			h.nextShown = !next.empty() && !(flags & HUDBAR_NEXT_GONE);
			h.notice = (flags & HUDBAR_NOTICE) != 0;
			h.nextUntil = h.nextShown && nextSeconds > 0 ? now + (float)nextSeconds : 0.0f;
			h.avatar = avatar;
			h.nextAvatar = nextAvatar;
		}
		m_Hud = h;
		events.push_back({ Event::HUDBAR, 0, 0, 0 });
		return true;
	}
	case OP_VARIANT:
	{
		const int menu = r.Byte();
		const int variant = r.Byte();
		if (r.Bad())
		{
			Fail("short VARIANT", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		if (m_bPending[menu])
			m_Pending[menu].variant = variant ? 1 : 0;
		if (m_Live[menu].complete)
		{
			m_Live[menu].variant = variant ? 1 : 0;
			events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_SELECT:
	{
		const int menu = r.Byte();
		const int index = r.Byte();
		const int item = r.Short();
		const int options = r.Byte();
		const int value = r.Byte();
		std::string label = r.String(LEN_SELECT_LABEL);
		std::string tip = r.String(LEN_CONTROL_TIP);
		if (r.Bad())
		{
			Fail("short SELECT", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS || !m_bPending[menu] || index >= MAX_MENU_SELECTS || options < 1 ||
		    options > MAX_SELECT_OPTIONS)
			return true;
		Menu &m = m_Pending[menu];
		if (item != NO_ITEM && item >= (int)m.items.size())
			return true;
		if (index >= (int)m.selects.size())
			m.selects.resize(index + 1);
		Select &sel = m.selects[index];
		size_t have = sel.label.size() + sel.tip.size();
		for (const std::string &o : sel.options)
			have += o.size();
		if (label.size() + tip.size() > have && !Fits(label.size() + tip.size() - have))
		{
			Fail("text limit", menu);
			return false;
		}
		sel = Select();
		sel.defined = true;
		sel.item = item;
		sel.label = label;
		sel.tip = tip;
		sel.options.resize(options);
		sel.value = value < options ? value : 0;
		return true;
	}
	case OP_SELECTOPTION:
	{
		const int menu = r.Byte();
		const int index = r.Byte();
		const int option = r.Byte();
		std::string label = r.String(LEN_OPTION);
		if (r.Bad())
		{
			Fail("short SELECTOPTION", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS || !m_bPending[menu] || index >= (int)m_Pending[menu].selects.size())
			return true;
		Select &sel = m_Pending[menu].selects[index];
		if (!sel.defined || option >= (int)sel.options.size())
			return true;
		if (label.size() > sel.options[option].size() && !Fits(label.size() - sel.options[option].size()))
		{
			Fail("text limit", menu);
			return false;
		}
		sel.options[option] = label;
		return true;
	}
	case OP_SELECTVALUE:
	{
		const int menu = r.Byte();
		const int index = r.Byte();
		const int value = r.Byte();
		if (r.Bad())
		{
			Fail("short SELECTVALUE", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->selects.size() || !m->selects[index].defined ||
			    value >= (int)m->selects[index].options.size())
				continue;
			m->selects[index].value = value;
			m->selects[index].updates++;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_CONFIRM:
	{
		const int menu = r.Byte();
		const int kind = r.Byte();
		const int index = r.Short();
		std::string question = r.String(LEN_CONFIRM);
		if (r.Bad())
		{
			Fail("short CONFIRM", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS || (kind != CONFIRM_ITEM && kind != CONFIRM_BUTTON))
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			std::string *have = nullptr;
			if (kind == CONFIRM_ITEM && m && index < (int)m->items.size() &&
			    m->items[index].defined)
				have = &m->items[index].confirm;
			else if (kind == CONFIRM_BUTTON && m && index < (int)m->buttonConfirms.size())
				have = &m->buttonConfirms[index];
			if (!have)
				continue;
			if (question.size() > have->size() && !Fits(question.size() - have->size()))
			{
				if (m == &m_Live[menu])
					continue; // a live change over the limit is dropped; the menu and the message stay
				Fail("text limit", menu);
				return false;
			}
			*have = question;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_BUTTONSTATE:
	{
		const int menu = r.Byte();
		const int index = r.Byte();
		const int flags = r.Byte();
		if (r.Bad())
		{
			Fail("short BUTTONSTATE", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->buttonFlags.size())
				continue;
			m->buttonFlags[index] = flags & BUTTON_FLAGS_ALL;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_STARS:
	{
		const int menu = r.Byte();
		const int tenths = r.Byte();
		std::string text = r.String(LEN_STARS);
		if (r.Bad())
		{
			Fail("short STARS", menu);
			return false;
		}
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m)
				continue;
			if (text.size() > m->starsText.size() && !Fits(text.size() - m->starsText.size()))
			{
				if (m == &m_Live[menu])
					continue; // a live change over the limit is dropped; the menu and the message stay
				Fail("text limit", menu);
				return false;
			}
			m->starsTenths = tenths <= 10 * MAX_STARS ? tenths : STARS_NONE;
			m->starsText = text;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	case OP_ITEMPICTURE:
	{
		const int menu = r.Byte();
		const int index = r.Short();
		std::string path = r.String(LEN_PATH);
		if (r.Bad())
		{
			Fail("short ITEMPICTURE", menu);
			return false;
		}
		if (!path.empty() && !ValidPicturePath(path))
			path.clear(); // drawn as no picture
		if (menu >= MAX_SERVER_MENUS)
			return true;
		Menu *targets[2] = { m_Live[menu].complete ? &m_Live[menu] : nullptr,
		                     m_bPending[menu] ? &m_Pending[menu] : nullptr };
		for (Menu *m : targets)
		{
			if (!m || index >= (int)m->items.size() || !m->items[index].defined)
				continue;
			std::string &have = m->items[index].image;
			if (path.size() > have.size() && !Fits(path.size() - have.size()))
			{
				if (m == &m_Live[menu])
					continue; // a live change over the limit is dropped; the menu and the message stay
				Fail("text limit", menu);
				return false;
			}
			have = path;
			if (m == &m_Live[menu])
				events.push_back({ Event::CHANGED, menu, 0, 0 });
		}
		return true;
	}
	default:
		return true;
	}
}
} // namespace servermenu
