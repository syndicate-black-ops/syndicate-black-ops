//========= SBO VGUI2: GameUI's option script files (settings.scr, user.scr) =========//
//
// Purpose: reads and writes the option scripts GameUI's Create Server dialog (settings.scr, "SERVER_OPTIONS") and
//          Multiplayer "Advanced..." dialog (user.scr, "INFO_OPTIONS") use, the same way HL25's GameUI does, so SBO's
//          own windows keep those files and the cvars exactly as GameUI would:
//            * the file format (CScriptObject / CDescription): VERSION 1.0, DESCRIPTION <type> { "cvar" { "Prompt"
//              { BOOL | NUMBER min max | STRING | LIST "label" "value" ... } { "default" } [SetInfo] } ... }
//            * the values sent to the engine (CScriptObject::WriteToConfig): `cvar "value"` or `setinfo cvar "value"`
//              per option; BOOL "1"/"0", NUMBER "%f" clamped to min/max (-1 = no limit), STRING without '"' and '%',
//              LIST the value of the item at index (int)atof(current) ("0.0" if none: GameUI's own rule, which
//              matches the chosen item when the values are 0..n-1 in list order)
//            * the file written back (CScriptObject::WriteToScriptFile, CRLF): the header GameUI writes, every object
//              with its current value as the new default
//          Plain C++ (no SDK or engine header): the parser is tested with ASan/UBSan (tests/script_file/).
//=================================================================//

#ifndef SBO_VGUI2_SBO_SCRIPT_FILE_H
#define SBO_VGUI2_SBO_SCRIPT_FILE_H

#include <string>
#include <vector>

namespace sbo_script
{
enum Type
{
	TYPE_BOOL = 1,
	TYPE_NUMBER,
	TYPE_LIST,
	TYPE_STRING,
};

struct ListItem
{
	std::string label;
	std::string value;
};

struct Object
{
	std::string cvar;
	std::string prompt;
	Type type = TYPE_STRING;
	float fMin = -1.0f, fMax = -1.0f; //!< NUMBER; -1 = no limit
	std::vector<ListItem> items;      //!< LIST
	std::string defValue;             //!< the file's default (the last value chosen)
	std::string curValue;             //!< the value to write (the default until set)
	bool setInfo = false;

	//! GameUI's GatherCurrentValues + SetCurValue: '"' and '%' removed; BOOL / NUMBER get trailing zeros and the '.'
	//! trimmed
	void SetCurValue(const std::string &value);
	//! The text sent to the engine for this object, e.g. `hostname "x"\n` (WriteToConfig)
	std::string ConfigCommand() const;
	//! For a LIST: the row GameUI selects ((int) of the default as a number), clamped; -1 if no items
	int ListRow() const;
};

struct Description
{
	enum Kind
	{
		KIND_SERVER, //!< settings.scr, DESCRIPTION SERVER_OPTIONS
		KIND_INFO,   //!< user.scr, DESCRIPTION INFO_OPTIONS
	};
	Kind kind = KIND_SERVER;
	std::vector<Object> objects;

	//! Parses a whole file; false if it is not a version 1.0 script of the expected kind (objects read so far kept).
	//! At most MAX_OBJECTS objects and MAX_ITEMS list items each; strings cut at MAX_STRING.
	bool Parse(const char *data, size_t size, Kind expected);
	//! The file as GameUI writes it. timeText: the "File generated" text (see FileTime)
	std::string Write(const std::string &timeText) const;
	Object *Find(const char *cvar);

	static constexpr size_t MAX_OBJECTS = 256;
	static constexpr size_t MAX_ITEMS = 128;
	static constexpr size_t MAX_STRING = 256;
};

//! The "File generated:" text GameUI writes: settings.scr 12-hour with AM/PM, user.scr 24-hour with " AM"
//! (CServerDescription / CInfoDescription::WriteScriptHeader). t: seconds since 1970 (local time)
std::string FileTime(long long t, Description::Kind kind);
//! A number as the windows show it: "300.000000" -> "300", "0.500000" -> "0.5" (other text unchanged)
std::string NumberText(const std::string &value);
//! '"' and '%' removed (GameUI's UTIL_StripInvalidCharacters / FixupString)
std::string StripInvalid(const std::string &s);
//! The value a console config file (config.cfg as the engine writes it: `name "value"` lines) gives the cvar `name`
//! when it is executed: the last `name value` command (case-insensitive name; commands end at a line break or a ';'
//! outside quotes; `//` comments skipped). False if no command sets it. The value is cut at MAX_STRING.
bool ConfigValue(const char *data, size_t size, const char *name, std::string &value);
} // namespace sbo_script

#endif // SBO_VGUI2_SBO_SCRIPT_FILE_H
