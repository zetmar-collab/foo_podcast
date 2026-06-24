#include "stdafx.h"
#include "xml_util.h"

namespace xml_util {

	CComPtr<IXMLDOMDocument> Load(const char* utf8xml) {
		CComPtr<IXMLDOMDocument> doc;
		if (FAILED(doc.CoCreateInstance(L"Msxml2.DOMDocument.6.0"))) {
			if (FAILED(doc.CoCreateInstance(L"MSXML2.DOMDocument"))) return nullptr;
		}
		doc->put_async(VARIANT_FALSE);
		doc->put_validateOnParse(VARIANT_FALSE);
		doc->put_resolveExternals(VARIANT_FALSE);

		pfc::stringcvt::string_wide_from_utf8 wide(utf8xml);
		CComBSTR bstr(wide.get_ptr());
		VARIANT_BOOL ok = VARIANT_FALSE;
		doc->loadXML(bstr, &ok);
		if (ok != VARIANT_TRUE) return nullptr;
		return doc;
	}

	pfc::string8 LocalName(IXMLDOMNode* node) {
		if (!node) return pfc::string8();
		CComBSTR name;
		node->get_nodeName(&name);
		pfc::string8 full = pfc::stringcvt::string_utf8_from_wide(name.m_str ? name.m_str : L"");
		t_size colon = full.find_last(':');
		if (colon == pfc::infinite_size) return full;
		pfc::string8 out;
		out.set_string(full.get_ptr() + colon + 1, full.length() - colon - 1);
		return out;
	}

	CComPtr<IXMLDOMNode> ChildByName(IXMLDOMNode* parent, const wchar_t* name) {
		if (!parent) return nullptr;
		CComPtr<IXMLDOMNodeList> children;
		parent->get_childNodes(&children);
		if (!children) return nullptr;
		long count = 0;
		children->get_length(&count);
		for (long i = 0; i < count; ++i) {
			CComPtr<IXMLDOMNode> child;
			children->get_item(i, &child);
			if (!child) continue;
			pfc::string8 local = LocalName(child);
			pfc::stringcvt::string_wide_from_utf8 localW(local.get_ptr());
			if (_wcsicmp(localW.get_ptr(), name) == 0) return child;
		}
		return nullptr;
	}

	void ChildrenByName(IXMLDOMNode* parent, const wchar_t* name, std::vector<CComPtr<IXMLDOMNode>>& out) {
		out.clear();
		if (!parent) return;
		CComPtr<IXMLDOMNodeList> children;
		parent->get_childNodes(&children);
		if (!children) return;
		long count = 0;
		children->get_length(&count);
		for (long i = 0; i < count; ++i) {
			CComPtr<IXMLDOMNode> child;
			children->get_item(i, &child);
			if (!child) continue;
			pfc::string8 local = LocalName(child);
			pfc::stringcvt::string_wide_from_utf8 localW(local.get_ptr());
			if (_wcsicmp(localW.get_ptr(), name) == 0) out.push_back(child);
		}
	}

	pfc::string8 Text(IXMLDOMNode* node) {
		if (!node) return pfc::string8();
		CComBSTR text;
		node->get_text(&text);
		if (!text.m_str) return pfc::string8();
		return pfc::string8(pfc::stringcvt::string_utf8_from_wide(text.m_str).get_ptr());
	}

	pfc::string8 ChildText(IXMLDOMNode* parent, const wchar_t* name) {
		CComPtr<IXMLDOMNode> child = ChildByName(parent, name);
		return Text(child);
	}

	pfc::string8 Attr(IXMLDOMNode* node, const wchar_t* name) {
		if (!node) return pfc::string8();
		CComPtr<IXMLDOMNamedNodeMap> attrs;
		node->get_attributes(&attrs);
		if (!attrs) return pfc::string8();
		CComPtr<IXMLDOMNode> attr;
		CComBSTR nameBstr(name);
		attrs->getNamedItem(nameBstr, &attr);
		return Text(attr);
	}

}
