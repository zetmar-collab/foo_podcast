#pragma once

namespace podcast_http {

	class exception_http : public pfc::exception {
	public:
		explicit exception_http(const char* msg) : pfc::exception(msg) {}
	};

	// Synchronous GET via WinHTTP. Throws exception_http on failure.
	pfc::string8 get(const char* url, abort_callback& p_abort);

}
