#include "stdafx.h"
#include "opml.h"
#include "xml_util.h"
#include <vector>

namespace opml {

	namespace {
		void RecurseOutlines(IXMLDOMNode* node, pfc::list_t<FeedRef>& out) {
			std::vector<CComPtr<IXMLDOMNode>> outlines;
			xml_util::ChildrenByName(node, L"outline", outlines);
			for (auto& outline : outlines) {
				pfc::string8 xmlUrl = xml_util::Attr(outline, L"xmlUrl");
				if (!xmlUrl.is_empty()) {
					FeedRef r;
					r.url = xmlUrl;
					r.title = xml_util::Attr(outline, L"title");
					if (r.title.is_empty()) r.title = xml_util::Attr(outline, L"text");
					out.add_item(r);
				}
				RecurseOutlines(outline, out);
			}
		}

		pfc::string8 EscapeXml(const char* s) {
			pfc::string8 out;
			for (const char* p = s; *p; ++p) {
				switch (*p) {
				case '&': out << "&amp;"; break;
				case '<': out << "&lt;"; break;
				case '>': out << "&gt;"; break;
				case '"': out << "&quot;"; break;
				default: out.add_char(*p); break;
				}
			}
			return out;
		}
	}

	void ParseOpml(const pfc::string8& xmlText, pfc::list_t<FeedRef>& out) {
		out.remove_all();
		xml_util::ComScope com;
		auto doc = xml_util::Load(xmlText.get_ptr());
		if (!doc) return;

		CComPtr<IXMLDOMElement> root;
		doc->get_documentElement(&root);
		if (!root) return;

		CComPtr<IXMLDOMNode> body = xml_util::ChildByName(root, L"body");
		if (!body) return;

		RecurseOutlines(body, out);
	}

	pfc::string8 BuildOpml(const podcast::Library& lib) {
		pfc::string8 out;
		out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n";
		out << "<opml version=\"2.0\">\r\n<head><title>foo_podcast subscriptions</title></head>\r\n<body>\r\n";
		for (t_size i = 0; i < lib.channels.get_count(); ++i) {
			const podcast::Channel& ch = lib.channels[i];
			out << "  <outline text=\"" << EscapeXml(ch.title.get_ptr()) << "\" title=\"" << EscapeXml(ch.title.get_ptr())
				<< "\" type=\"rss\" xmlUrl=\"" << EscapeXml(ch.feedUrl.get_ptr()) << "\"/>\r\n";
		}
		out << "</body>\r\n</opml>\r\n";
		return out;
	}

}
