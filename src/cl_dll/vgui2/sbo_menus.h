//========= SBO VGUI2 port: menu infrastructure =========//
//
// Purpose: Shared infrastructure of the VGUI1 -> VGUI2 menu migration (docs/dev/VGUI2_MIGRATION.md):
//          * VGui2_Menus_Init: creates the menu panels
//          * SBO's UI strings: ui/resource/language/sbo_ui_<language>.txt (#SBO_* tokens)
//          * SBO's VGUI2 scheme: ui/resource/sboscheme.res (#base clientschemebase.res + SBO colours/scrollbars)
//          * helpers to reproduce the VGUI1 look: XRES()/YRES() layout, VGUI1 colours, VGUI1 text scheme fonts
//
// The widgets built on this are in sbo_widgets.h. vgui2_api.h has the SBO -> VGUI2 entry points.
//
// SDK world (hl1_source_sdk headers); SBO data through sbo_bridge.h.
//=================================================================//

#ifndef SBO_VGUI2_SBO_MENUS_H
#define SBO_VGUI2_SBO_MENUS_H

#include <wchar.h>
#include <Color.h>
#include <vgui/VGUI2.h>

namespace sboui
{
//! SBO's VGUI1 colour for borders and window titles (LineBorder( Color( 41, 107, 34, 0 ) ) all over SBO's menus).
const Color SBO_GREEN(41, 107, 34, 255);

//! The VGUI1 layout macros (cl_util.h XRES/YRES: 640x480 basis, rounded) for SBO's current screen size.
int XRes(float x);
int YRes(float y);

//! A VGUI1 colour: alpha 0 = opaque, 255 = invisible (VGUI1's convention, e.g. CTransparentPanel's iTrans).
inline Color InvAlphaColor(int r, int g, int b, int a)
{
	return Color(r, g, b, 255 - a);
}

//! VGUI2 font equal to the VGUI1 font of a text scheme ("Primary Button Text", "Title Font", "Briefing Text", ...;
//! gamedir/syndicate/<res>_textscheme.txt, the file the SDK's VGUI1 viewport loaded): same typeface, size (pixel
//! height) and weight. Fonts are created once and cached. vgui2::INVALID_FONT while SBO's viewport doesn't exist.
vgui2::HFont GetTextSchemeFont(const char *schemeName);

//! Foreground colour of a VGUI1 text scheme (pSchemes->getFgColor( hScheme ) in the VGUI1 code), VGUI2 alpha.
//! False (out unchanged) while SBO's viewport doesn't exist.
bool GetTextSchemeFgColor(const char *schemeName, Color &out);

//! The VGUI1 font every VGUI1 Label/TextPanel/Button without setFont() uses (Scheme::sf_primary1, which the
//! viewport sets to "Primary Button Text").
constexpr const char *DEFAULT_TEXT_SCHEME = "Primary Button Text";

//! SBO's VGUI2 scheme (ui/resource/sboscheme.res, tag "SboScheme"); 0 before VGui2_Menus_Init.
vgui2::HScheme GetScheme();

//! Text from SBO's engine/server side (MOTD, map .txt files, ...) to a wide string: UTF-8 if the bytes are valid
//! UTF-8, else Windows-1252/Latin-1 bytes (what VGUI1 drew). "\r\n" and lone "\r" become "\n".
void TextToWide(const char *text, wchar_t *out, int outSizeInBytes);

//! Localized string of a #SBO_* token (sbo_ui_<language>.txt); a literal backslash-n in it becomes a new line.
//! Falls back to the token itself (without '#') if it isn't in any language file.
void Localize(const char *token, wchar_t *out, int outSizeInBytes);
} // namespace sboui

#endif // SBO_VGUI2_SBO_MENUS_H
