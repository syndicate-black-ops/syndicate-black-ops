//========= SBO VGUI2: map pictures (server browser, joining screen) =========//
//
// Purpose: see sbo_map_picture.h.
//=================================================================//

#include <stdio.h>
#include <string.h>

#include <FileSystem.h>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

#include "client_steam_context.h"
#include "sbo_image_decode.h"
#include "sbo_map_picture.h"
#include "sbo_ui_style.h"

namespace
{
// map pictures are scaled down to at most this (the shipped and downloaded pictures are 512x288)
constexpr int PICTURE_MAX_WIDE = 512;
constexpr int PICTURE_MAX_TALL = 384;
// The map pictures: shipped in the mod as gfx/mapimages/<map>.jpg; a map without one is downloaded once from
// MAP_IMAGE_URL (%s = the map name; GitHub Pages of github.com/syndicate-black-ops/map-images, where mappers add
// pictures by pull request) into syndicate_downloads/gfx/mapimages/ (the "GAMEDOWNLOAD" path), where the same file
// name finds it next time.
constexpr const char MAP_IMAGE_URL[] = "https://syndicate-black-ops.github.io/map-images/%s.jpg"; // 512x288 JPEG, lower case name
constexpr const char MAP_IMAGE_DIR[] = "gfx/mapimages";
constexpr unsigned MAP_IMAGE_TIMEOUT_S = 15;

//! A whole file through the engine's file system (nullptr pathID: every search path), at most MAX_FILE_BYTES
bool ReadGameFile(const char *path, std::vector<uint8_t> &data)
{
	data.clear();
	// every search path; the downloads folder (GAMEDOWNLOAD, where the downloaded pictures are kept) asked for by name too:
	// the plain search did not find files there on a Windows install (pictures downloaded in an earlier game
	// showed only after a new download)
	FileHandle_t fh = g_pFullFileSystem->FileExists(path) ? g_pFullFileSystem->Open(path, "rb") : nullptr;
	if (!fh)
		fh = g_pFullFileSystem->Open(path, "rb", "GAMEDOWNLOAD");
	if (!fh)
		return false;
	const unsigned size = g_pFullFileSystem->Size(fh);
	if (size > 0 && size <= sbo_image::MAX_FILE_BYTES)
	{
		data.resize(size);
		if (g_pFullFileSystem->Read(data.data(), (int)size, fh) != (int)size)
			data.clear();
	}
	g_pFullFileSystem->Close(fh);
	return !data.empty();
}

int MakeTexture(const std::vector<uint8_t> &rgba, int w, int h)
{
	const int texture = vgui2::surface()->CreateNewTextureID(true);
	vgui2::surface()->DrawSetTextureRGBA(texture, rgba.data(), w, h, true, true);
	return texture;
}
} // namespace

CSboMapPictures::~CSboMapPictures()
{
	if (m_hHttp != INVALID_HTTPREQUEST_HANDLE && ClientSteamContext().SteamHTTP())
		ClientSteamContext().SteamHTTP()->ReleaseHTTPRequest(m_hHttp);
	for (const Picture &o : m_Pictures)
	{
		if (o.texture != -1 && !o.isPlaceholder)
			vgui2::surface()->DeleteTextureByID(o.texture);
	}
	if (m_iPlaceholder >= 0)
		vgui2::surface()->DeleteTextureByID(m_iPlaceholder);
}

CSboMapPictures::Picture *CSboMapPictures::Find(const std::string &map)
{
	for (Picture &o : m_Pictures)
	{
		if (o.map == map)
			return &o;
	}
	return nullptr;
}

const CSboMapPictures::Picture *CSboMapPictures::Get(const std::string &map)
{
	if (Picture *pFound = Find(map))
	{
		pFound->lastUse = ++m_uUse;
		return pFound;
	}
	if (m_Pictures.size() >= MAX_PICTURES)
	{
		// the least recently used picture goes (not the one downloading); callers never keep the pointers
		int oldest = -1;
		for (int i = 0; i < (int)m_Pictures.size(); i++)
		{
			if (m_Pictures[i].map == m_HttpMap && m_hHttp != INVALID_HTTPREQUEST_HANDLE)
				continue;
			if (oldest < 0 || m_Pictures[i].lastUse < m_Pictures[oldest].lastUse)
				oldest = i;
		}
		if (oldest < 0)
			return nullptr;
		if (m_Pictures[oldest].texture != -1 && !m_Pictures[oldest].isPlaceholder)
			vgui2::surface()->DeleteTextureByID(m_Pictures[oldest].texture);
		m_Pictures.erase(m_Pictures.begin() + oldest);
	}

	Picture o;
	o.map = map;
	if (sbolook::SafeMapName(map))
	{
		std::vector<uint8_t> data, rgba;
		int w = 0, h = 0;
		char path[128];
		// 1. the map picture: shipped, or downloaded earlier
		snprintf(path, sizeof(path), "%s/%s.jpg", MAP_IMAGE_DIR, map.c_str());
		if (ReadGameFile(path, data) && sbo_image::DecodeJpegScaled(data.data(), data.size(), PICTURE_MAX_WIDE, PICTURE_MAX_TALL, rgba, w, h))
		{
			o.texture = MakeTexture(rgba, w, h);
			o.wide = w;
			o.tall = h;
			o.isMapImage = true;
		}
		else
		{
			// 2. meanwhile (and if the download fails): the spectator overview SBO ships (pure green = its empty
			// background, drawn transparent)
			snprintf(path, sizeof(path), "overviews/%s.tga", map.c_str());
			if (ReadGameFile(path, data)
			    && sbo_image::DecodeTgaScaled(data.data(), data.size(), PICTURE_MAX_WIDE, PICTURE_MAX_TALL, rgba, w, h, true))
			{
				o.texture = MakeTexture(rgba, w, h);
				o.wide = w;
				o.tall = h;
			}
			o.wantDownload = true;
		}
	}
	if (o.texture == -1)
	{
		// 3. nothing (yet): SBO's placeholder, one texture for the whole cache
		if (m_iPlaceholder == -2)
		{
			m_iPlaceholder = -1;
			std::vector<uint8_t> data, rgba;
			int w = 0, h = 0;
			if (ReadGameFile("gfx/sbo_nomappicture.jpg", data)
			    && sbo_image::DecodeJpegScaled(data.data(), data.size(), PICTURE_MAX_WIDE, PICTURE_MAX_TALL, rgba, w, h))
			{
				m_iPlaceholder = MakeTexture(rgba, w, h);
				m_iPlaceholderW = w;
				m_iPlaceholderH = h;
			}
		}
		if (m_iPlaceholder >= 0)
		{
			o.texture = m_iPlaceholder;
			o.wide = m_iPlaceholderW;
			o.tall = m_iPlaceholderH;
			o.isPlaceholder = true;
		}
	}
	o.lastUse = ++m_uUse;
	m_Pictures.push_back(o);
	return &m_Pictures.back();
}

void CSboMapPictures::StartDownload(const std::string &map)
{
	Picture *pPic = Find(map);
	ISteamHTTP *pHttp = ClientSteamContext().SteamHTTP();
	if (!pPic || !pPic->wantDownload || m_hHttp != INVALID_HTTPREQUEST_HANDLE || !pHttp || !sbolook::SafeMapName(map))
		return;
	pPic->wantDownload = false; // one request per map and session
	char url[256];
	snprintf(url, sizeof(url), MAP_IMAGE_URL, map.c_str());
	HTTPRequestHandle h = pHttp->CreateHTTPRequest(k_EHTTPMethodGET, url);
	if (h == INVALID_HTTPREQUEST_HANDLE)
		return;
	pHttp->SetHTTPRequestNetworkActivityTimeout(h, MAP_IMAGE_TIMEOUT_S);
	// JPEG only: a CDN in front of the host may send WebP to a client that accepts it
	pHttp->SetHTTPRequestHeaderValue(h, "Accept", "image/jpeg");
	SteamAPICall_t call = k_uAPICallInvalid;
	if (!pHttp->SendHTTPRequest(h, &call) || call == k_uAPICallInvalid)
	{
		pHttp->ReleaseHTTPRequest(h);
		return;
	}
	m_hHttp = h;
	m_hHttpCall = call;
	m_HttpMap = map;
}

bool CSboMapPictures::Poll()
{
	ISteamHTTP *pHttp = ClientSteamContext().SteamHTTP();
	ISteamUtils *pUtils = ClientSteamContext().SteamUtils();
	if (m_hHttp == INVALID_HTTPREQUEST_HANDLE || !pHttp || !pUtils)
		return false;
	bool failed = false;
	if (!pUtils->IsAPICallCompleted(m_hHttpCall, &failed))
		return false;
	HTTPRequestCompleted_t result = {};
	const bool got = !failed && pUtils->GetAPICallResult(m_hHttpCall, &result, sizeof(result), HTTPRequestCompleted_t::k_iCallback, &failed)
	                 && !failed;
	std::vector<uint8_t> body;
	uint32 size = 0;
	// a JPEG answer only (a missing map is a 404 with an HTML page)
	char contentType[64] = "";
	uint32 typeSize = 0;
	if (got && pHttp->GetHTTPResponseHeaderSize(m_hHttp, "Content-Type", &typeSize) && typeSize > 0
	    && typeSize < sizeof(contentType))
		pHttp->GetHTTPResponseHeaderValue(m_hHttp, "Content-Type", (uint8 *)contentType, sizeof(contentType));
	contentType[sizeof(contentType) - 1] = 0;
	if (got && result.m_bRequestSuccessful && result.m_eStatusCode == k_EHTTPStatusCode200OK
	    && !Q_strnicmp(contentType, "image/jpeg", 10)
	    && pHttp->GetHTTPResponseBodySize(m_hHttp, &size) && size > 0 && size <= sbo_image::MAX_FILE_BYTES)
	{
		body.resize(size);
		if (!pHttp->GetHTTPResponseBodyData(m_hHttp, body.data(), size))
			body.clear();
	}
	pHttp->ReleaseHTTPRequest(m_hHttp);
	m_hHttp = INVALID_HTTPREQUEST_HANDLE;
	m_hHttpCall = k_uAPICallInvalid;

	std::vector<uint8_t> rgba;
	int w = 0, h = 0;
	Picture *pPic = Find(m_HttpMap);
	if (!pPic || body.empty() || !sbo_image::DecodeJpegScaled(body.data(), body.size(), PICTURE_MAX_WIDE, PICTURE_MAX_TALL, rgba, w, h))
		return false; // not a JPEG (or none): the overview / map name stays

	// keep it for next time: syndicate_downloads/gfx/mapimages/<map>.jpg (only a file that decoded)
	char path[128];
	snprintf(path, sizeof(path), "%s/%s.jpg", MAP_IMAGE_DIR, m_HttpMap.c_str());
	g_pFullFileSystem->CreateDirHierarchy(MAP_IMAGE_DIR, "GAMEDOWNLOAD");
	if (FileHandle_t fh = g_pFullFileSystem->Open(path, "wb", "GAMEDOWNLOAD"))
	{
		g_pFullFileSystem->Write(body.data(), (int)body.size(), fh);
		g_pFullFileSystem->Close(fh);
	}

	if (pPic->texture != -1 && !pPic->isPlaceholder)
		vgui2::surface()->DeleteTextureByID(pPic->texture);
	pPic->isPlaceholder = false;
	pPic->texture = MakeTexture(rgba, w, h);
	pPic->wide = w;
	pPic->tall = h;
	pPic->isMapImage = true;
	return true;
}
