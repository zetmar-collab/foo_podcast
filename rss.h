#pragma once

#include "model.h"

namespace rss {

	// Parses an RSS 2.0 or Atom feed document into a Channel (title +
	// episodes). Returns false if the document isn't recognizable as a feed.
	// Does not set Channel::feedUrl -- the caller knows that already.
	bool ParseFeed(const pfc::string8& xmlText, podcast::Channel& out);

}
