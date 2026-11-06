//========= SBO VGUI2: map pictures (server browser, joining screen) =========//
//
// Purpose: a picture of a map as a VGUI2 texture, for the server browser's right panel and the joining screen:
//            1. gfx/mapimages/<map>.jpg: shipped for the stock maps, or downloaded earlier (syndicate_downloads/);
//            2. for any other map: downloaded once per game from MAP_IMAGE_URL (Steam's HTTP, JPEG only) and kept in
//               syndicate_downloads/gfx/mapimages/, where (1) finds it next time;
//            3. meanwhile, or when there is none: the spectator overview overviews/<map>.tga (pure green drawn
//               transparent); else SBO's placeholder gfx/sbo_nomappicture.jpg (isPlaceholder: the caller draws the
//               map name on it); without that file no texture (the caller shows the map name).
//          The files are decoded by sbo_image_decode (the engine's surface reads no JPEG and no TGA above 256x256).
//          One download at a time; at most MAX_PICTURES maps per cache.
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_MAP_PICTURE_H
#define SBO_VGUI2_SBO_MAP_PICTURE_H

#include <string>
#include <vector>

#include <steam/steam_api.h>
#include <steam/isteamhttp.h>

class CSboMapPictures
{
public:
	struct Picture
	{
		std::string map;
		int texture = -1; //!< -1: none (show the map name)
		int wide = 0, tall = 0;
		bool isMapImage = false;   //!< the map picture (not the overview)
		bool isPlaceholder = false; //!< SBO's "no map picture" image (gfx/sbo_nomappicture.jpg): draw the map name on it
		bool wantDownload = false; //!< the mod has no map picture: download it once
		unsigned lastUse = 0;      //!< Get's counter when it was last asked for (the oldest goes when full)
	};
	enum { MAX_PICTURES = 48 };

	~CSboMapPictures();

	//! The picture for the map (lower case name; made on first use). When the cache is full the picture asked for
	//! least recently goes (its texture freed); nullptr only if nothing can go.
	const Picture *Get(const std::string &map);
	//! The map's picture is made already (Get would not read a file)
	bool Has(const std::string &map) { return Find(map) != nullptr; }
	//! Starts the map's download if it has no map picture yet and no other download runs.
	void StartDownload(const std::string &map);
	//! Polls the running download; true when a picture changed (repaint).
	bool Poll();
	bool IsDownloading() const { return m_hHttp != INVALID_HTTPREQUEST_HANDLE; }

private:
	Picture *Find(const std::string &map);

	std::vector<Picture> m_Pictures; // Get's pointer is used at once, never kept
	HTTPRequestHandle m_hHttp = INVALID_HTTPREQUEST_HANDLE;
	SteamAPICall_t m_hHttpCall = k_uAPICallInvalid;
	std::string m_HttpMap;
	unsigned m_uUse = 0;
	int m_iPlaceholder = -2; //!< the placeholder's texture (one for every map of this cache), -2 not loaded, -1 none
	int m_iPlaceholderW = 0, m_iPlaceholderH = 0;
};

#endif // SBO_VGUI2_SBO_MAP_PICTURE_H
