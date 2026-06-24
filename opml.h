#pragma once

#include "model.h"

namespace opml {

	struct FeedRef {
		pfc::string8 url;
		pfc::string8 title;
	};

	// Extracts every outline element with an xmlUrl attribute, recursively.
	void ParseOpml(const pfc::string8& xmlText, pfc::list_t<FeedRef>& out);

	// Builds an OPML document listing every subscribed channel.
	pfc::string8 BuildOpml(const podcast::Library& lib);

}
