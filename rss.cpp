#include "stdafx.h"
#include "rss.h"
#include "xml_util.h"
#include <vector>

namespace rss {

	namespace {
		const char* kMonths[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };

		time_t ParseDate(const pfc::string8& s) {
			if (s.is_empty()) return 0;
			const char* p = s.get_ptr();
			const char* comma = strchr(p, ',');
			if (comma) p = comma + 1;
			while (*p == ' ') ++p;

			int day = 0, year = 0, hour = 0, minute = 0, sec = 0;
			char monStr[8] = {};
			if (sscanf(p, "%d %3s %d %d:%d:%d", &day, monStr, &year, &hour, &minute, &sec) == 6) {
				int mon = -1;
				for (int i = 0; i < 12; ++i) if (_stricmp(monStr, kMonths[i]) == 0) { mon = i; break; }
				if (mon >= 0) {
					struct tm tmv = {};
					tmv.tm_mday = day; tmv.tm_mon = mon; tmv.tm_year = year - 1900;
					tmv.tm_hour = hour; tmv.tm_min = minute; tmv.tm_sec = sec;
					return _mkgmtime(&tmv);
				}
			}

			int y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0;
			if (sscanf(p, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) >= 3) {
				struct tm tmv = {};
				tmv.tm_year = y - 1900; tmv.tm_mon = mo - 1; tmv.tm_mday = d;
				tmv.tm_hour = h; tmv.tm_min = mi; tmv.tm_sec = se;
				return _mkgmtime(&tmv);
			}
			return 0;
		}

		long ParseDuration(const pfc::string8& s) {
			if (s.is_empty()) return 0;
			int colons = 0;
			for (const char* p = s.get_ptr(); *p; ++p) if (*p == ':') ++colons;
			if (colons == 0) return atol(s.get_ptr());
			if (colons == 1) {
				int m = 0, sec = 0;
				if (sscanf(s.get_ptr(), "%d:%d", &m, &sec) == 2) return m * 60 + sec;
			} else {
				int h = 0, m = 0, sec = 0;
				if (sscanf(s.get_ptr(), "%d:%d:%d", &h, &m, &sec) == 3) return h * 3600 + m * 60 + sec;
			}
			return 0;
		}

		void ParseRssItems(IXMLDOMNode* channel, podcast::Channel& out) {
			std::vector<CComPtr<IXMLDOMNode>> items;
			xml_util::ChildrenByName(channel, L"item", items);
			for (auto& item : items) {
				podcast::Episode ep;
				ep.title = xml_util::ChildText(item, L"title");
				ep.guid = xml_util::ChildText(item, L"guid");

				CComPtr<IXMLDOMNode> enclosure = xml_util::ChildByName(item, L"enclosure");
				ep.url = xml_util::Attr(enclosure, L"url");
				if (ep.url.is_empty()) ep.url = xml_util::ChildText(item, L"link");
				if (ep.url.is_empty()) continue;

				if (ep.guid.is_empty()) ep.guid = ep.url;

				ep.pubDate = ParseDate(xml_util::ChildText(item, L"pubDate"));
				ep.durationSeconds = ParseDuration(xml_util::ChildText(item, L"duration"));

				out.episodes.add_item(ep);
			}
		}

		void ParseAtomEntries(IXMLDOMNode* feed, podcast::Channel& out) {
			std::vector<CComPtr<IXMLDOMNode>> entries;
			xml_util::ChildrenByName(feed, L"entry", entries);
			for (auto& entry : entries) {
				podcast::Episode ep;
				ep.title = xml_util::ChildText(entry, L"title");
				ep.guid = xml_util::ChildText(entry, L"id");

				std::vector<CComPtr<IXMLDOMNode>> links;
				xml_util::ChildrenByName(entry, L"link", links);
				pfc::string8 enclosureUrl, altUrl;
				for (auto& link : links) {
					pfc::string8 rel = xml_util::Attr(link, L"rel");
					pfc::string8 href = xml_util::Attr(link, L"href");
					if (href.is_empty()) continue;
					if (rel.equals("enclosure")) enclosureUrl = href;
					else if (altUrl.is_empty()) altUrl = href;
				}
				ep.url = enclosureUrl.is_empty() ? altUrl : enclosureUrl;
				if (ep.url.is_empty()) continue;
				if (ep.guid.is_empty()) ep.guid = ep.url;

				pfc::string8 dateStr = xml_util::ChildText(entry, L"published");
				if (dateStr.is_empty()) dateStr = xml_util::ChildText(entry, L"updated");
				ep.pubDate = ParseDate(dateStr);
				ep.durationSeconds = 0;

				out.episodes.add_item(ep);
			}
		}
	}

	bool ParseFeed(const pfc::string8& xmlText, podcast::Channel& out) {
		xml_util::ComScope com;
		auto doc = xml_util::Load(xmlText.get_ptr());
		if (!doc) return false;

		CComPtr<IXMLDOMElement> root;
		doc->get_documentElement(&root);
		if (!root) return false;

		pfc::string8 rootName = xml_util::LocalName(root);

		if (rootName.equals("rss")) {
			CComPtr<IXMLDOMNode> channel = xml_util::ChildByName(root, L"channel");
			if (!channel) return false;
			out.title = xml_util::ChildText(channel, L"title");
			ParseRssItems(channel, out);
			return true;
		}

		if (rootName.equals("feed")) {
			out.title = xml_util::ChildText(root, L"title");
			ParseAtomEntries(root, out);
			return true;
		}

		return false;
	}

}
