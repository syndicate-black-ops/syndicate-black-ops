//========= SBO VGUI2: the updater's HTTP (Steam) =========//
//
// Purpose: sbo_update::IHttp (src/cl_dll/updater/updater_client.h) on Steam's HTTP (ISteamHTTP), as the map pictures
//          use it (sbo_map_picture.cpp): asynchronous inside Steam, polled each frame, never blocking; HTTPS with
//          Steam's certificate checks; Windows and Linux alike. Each request has Steam's network activity timeout
//          (5 s without data) and an absolute timeout (the request's deadline).
//
// SDK world (hl1_source_sdk headers).
//==========================================================//

#include <stdio.h>
#include <string>

#include "client_steam_context.h"
#include <steam/isteamhttp.h>
#include <steam/isteamutils.h>

#include "../updater/updater_client.h"

namespace
{
constexpr uint32 NETWORK_ACTIVITY_TIMEOUT_S = 5;

class CSboSteamHttp : public sbo_update::IHttp
{
public:
	bool Available() override { return ClientSteamContext().SteamHTTP() && ClientSteamContext().SteamUtils(); }

	bool Start(const sbo_update::HttpRequest &req, std::string &err) override
	{
		Cancel();
		ISteamHTTP *http = ClientSteamContext().SteamHTTP();
		if (!http)
		{
			err = "no Steam HTTP";
			return false;
		}
		HTTPRequestHandle h = http->CreateHTTPRequest(k_EHTTPMethodGET, req.url.c_str());
		if (h == INVALID_HTTPREQUEST_HANDLE)
		{
			err = "bad request";
			return false;
		}
		http->SetHTTPRequestNetworkActivityTimeout(h, NETWORK_ACTIVITY_TIMEOUT_S);
		http->SetHTTPRequestAbsoluteTimeoutMS(h, (uint32)req.timeoutSeconds * 1000);
		http->SetHTTPRequestUserAgentInfo(h, "SyndicateBlackOps-Updater");
		for (const auto &hv : req.headers)
			http->SetHTTPRequestHeaderValue(h, hv.first.c_str(), hv.second.c_str());
		if (req.range)
		{
			char range[64];
			snprintf(range, sizeof(range), "bytes=%llu-%llu", (unsigned long long)req.rangeFirst,
			    (unsigned long long)req.rangeLast);
			http->SetHTTPRequestHeaderValue(h, "Range", range);
		}
		SteamAPICall_t call = k_uAPICallInvalid;
		if (!http->SendHTTPRequest(h, &call) || call == k_uAPICallInvalid)
		{
			http->ReleaseHTTPRequest(h);
			err = "cannot send";
			return false;
		}
		m_hRequest = h;
		m_hCall = call;
		m_MaxBytes = req.maxBytes;
		return true;
	}

	bool Poll(sbo_update::HttpResult &out) override
	{
		ISteamHTTP *http = ClientSteamContext().SteamHTTP();
		ISteamUtils *utils = ClientSteamContext().SteamUtils();
		if (m_hRequest == INVALID_HTTPREQUEST_HANDLE)
			return false;
		out = sbo_update::HttpResult();
		if (!http || !utils)
		{
			m_hRequest = INVALID_HTTPREQUEST_HANDLE;
			out.error = "Steam went away";
			return true;
		}
		bool failed = false;
		if (!utils->IsAPICallCompleted(m_hCall, &failed))
			return false;
		HTTPRequestCompleted_t result = {};
		const bool got = !failed &&
		                 utils->GetAPICallResult(m_hCall, &result, sizeof(result), HTTPRequestCompleted_t::k_iCallback, &failed) &&
		                 !failed;
		if (!got || !result.m_bRequestSuccessful)
			out.error = result.m_eStatusCode == k_EHTTPStatusCodeInvalid || !got ? "no response (offline or timed out)"
			                                                                       : "request failed";
		else
		{
			out.ok = true;
			out.status = (int)result.m_eStatusCode;
			uint32 size = 0;
			if (http->GetHTTPResponseBodySize(m_hRequest, &size) && size > 0)
			{
				if (size > m_MaxBytes)
				{
					out.ok = false;
					out.error = "response too large (" + std::to_string(size) + " bytes)";
				}
				else
				{
					out.body.resize(size);
					if (!http->GetHTTPResponseBodyData(m_hRequest, (uint8 *)&out.body[0], size))
					{
						out.ok = false;
						out.error = "cannot read the response";
						out.body.clear();
					}
				}
			}
			uint32 locSize = 0;
			if (http->GetHTTPResponseHeaderSize(m_hRequest, "Location", &locSize) && locSize > 0 && locSize < 4096)
			{
				std::string loc(locSize, '\0');
				if (http->GetHTTPResponseHeaderValue(m_hRequest, "Location", (uint8 *)&loc[0], locSize))
					out.location.assign(loc.c_str());
			}
		}
		http->ReleaseHTTPRequest(m_hRequest);
		m_hRequest = INVALID_HTTPREQUEST_HANDLE;
		m_hCall = k_uAPICallInvalid;
		return true;
	}

	void Cancel() override
	{
		ISteamHTTP *http = ClientSteamContext().SteamHTTP();
		if (m_hRequest != INVALID_HTTPREQUEST_HANDLE && http)
			http->ReleaseHTTPRequest(m_hRequest);
		m_hRequest = INVALID_HTTPREQUEST_HANDLE;
		m_hCall = k_uAPICallInvalid;
	}

private:
	HTTPRequestHandle m_hRequest = INVALID_HTTPREQUEST_HANDLE;
	SteamAPICall_t m_hCall = k_uAPICallInvalid;
	size_t m_MaxBytes = 0;
};
} // namespace

sbo_update::IHttp *SboUpdate_SteamHttp()
{
	static CSboSteamHttp s_Http;
	return &s_Http;
}
