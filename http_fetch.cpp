#include "stdafx.h"
#include "http_fetch.h"

namespace podcast_http {

	namespace {
		struct HandleGuard {
			HINTERNET h = nullptr;
			~HandleGuard() { if (h) WinHttpCloseHandle(h); }
		};
	}

	pfc::string8 get(const char* url, abort_callback& p_abort) {
		p_abort.check();

		pfc::stringcvt::string_wide_from_utf8 urlW(url);

		wchar_t hostName[256] = {};
		wchar_t urlPath[4096] = {};
		wchar_t extraInfo[2048] = {};

		URL_COMPONENTS uc = {};
		uc.dwStructSize = sizeof(uc);
		uc.lpszHostName = hostName;
		uc.dwHostNameLength = 256;
		uc.lpszUrlPath = urlPath;
		uc.dwUrlPathLength = 4096;
		uc.lpszExtraInfo = extraInfo;
		uc.dwExtraInfoLength = 2048;

		if (!WinHttpCrackUrl(urlW.get_ptr(), 0, 0, &uc)) {
			throw exception_http("Invalid URL.");
		}

		pfc::string8 pathAndQuery = pfc::stringcvt::string_utf8_from_wide(urlPath);
		pathAndQuery << pfc::stringcvt::string_utf8_from_wide(extraInfo).get_ptr();
		pfc::stringcvt::string_wide_from_utf8 pathW(pathAndQuery.get_ptr());

		HandleGuard session;
		session.h = WinHttpOpen(L"foo_podcast/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session.h) throw exception_http("Could not initialize WinHTTP.");
		WinHttpSetTimeouts(session.h, 10000, 10000, 15000, 20000);

		HandleGuard connect;
		connect.h = WinHttpConnect(session.h, uc.lpszHostName, uc.nPort, 0);
		if (!connect.h) throw exception_http("Could not connect to host.");

		DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
		HandleGuard request;
		request.h = WinHttpOpenRequest(connect.h, L"GET", pathW.get_ptr(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
		if (!request.h) throw exception_http("Could not create HTTP request.");

		p_abort.check();

		if (!WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
			throw exception_http("Could not send HTTP request.");
		}
		if (!WinHttpReceiveResponse(request.h, nullptr)) {
			throw exception_http("Could not receive HTTP response.");
		}

		DWORD statusCode = 0, statusSize = sizeof(statusCode);
		WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, nullptr);
		if (statusCode < 200 || statusCode >= 300) {
			pfc::string8 msg = "HTTP error ";
			msg << (int)statusCode << " for " << url;
			throw exception_http(msg.get_ptr());
		}

		pfc::string8 result;
		for (;;) {
			p_abort.check();
			DWORD available = 0;
			if (!WinHttpQueryDataAvailable(request.h, &available)) throw exception_http("HTTP read error.");
			if (available == 0) break;

			pfc::array_t<char> buf;
			buf.set_size(available);
			DWORD read = 0;
			if (!WinHttpReadData(request.h, buf.get_ptr(), available, &read)) throw exception_http("HTTP read error.");
			if (read == 0) break;
			result.add_string(buf.get_ptr(), read);
		}

		return result;
	}

}
