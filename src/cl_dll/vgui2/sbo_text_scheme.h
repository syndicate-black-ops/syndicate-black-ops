//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SBO_TEXT_SCHEME_H
#define SBO_TEXT_SCHEME_H

// SBO's text scheme: see sbo_text_scheme.cpp (the HL SDK's CSchemeManager without VGUI1).

// handle to an individual scheme
typedef int SchemeHandle_t;


//-----------------------------------------------------------------------------
// Purpose: Handles the loading of text scheme description from disk
//			supports different font/color/size schemes at different resolutions 
//-----------------------------------------------------------------------------
class CSchemeManager
{
public:
	// initialization
	CSchemeManager( int xRes, int yRes );
	virtual ~CSchemeManager();

	// scheme handling
	SchemeHandle_t getSchemeHandle( const char *schemeName );

	// getting info from schemes
	void getFgColor( SchemeHandle_t schemeHandle, int &r, int &g, int &b, int &a );
	void getBgColor( SchemeHandle_t schemeHandle, int &r, int &g, int &b, int &a );
	void getFgArmedColor( SchemeHandle_t schemeHandle, int &r, int &g, int &b, int &a );
	void getBgArmedColor( SchemeHandle_t schemeHandle, int &r, int &g, int &b, int &a );
	void getFgMousedownColor( SchemeHandle_t schemeHandle, int &r, int &g, int &b, int &a );
	void getBgMousedownColor( SchemeHandle_t schemeHandle, int &r, int &g, int &b, int &a );
	void getBorderColor( SchemeHandle_t schemeHandle, int &r, int &g, int &b, int &a );
	// the font description of a scheme (the VGUI2 widgets build their fonts from it: vgui2/sbo_menu_bridge.cpp)
	void getFontInfo( SchemeHandle_t schemeHandle, const char *&fontName, int &fontSize, int &fontWeight );
	// the screen width the loaded <width>_textscheme.txt was written for (0: none loaded, the defaults)
	int getFileRes() const { return m_iFileRes; }

private:
	class CScheme;
	CScheme *m_pSchemeList;
	int m_iNumSchemes;

	// Resolution we were initted at.
	int		m_xRes;
	// <width> of the loaded <width>_textscheme.txt
	int		m_iFileRes;

	CScheme *getSafeScheme( SchemeHandle_t schemeHandle );
};

#endif // SBO_TEXT_SCHEME_H
