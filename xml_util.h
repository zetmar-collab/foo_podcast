#pragma once

#include <vector>

namespace xml_util {

	// Initializes COM (multithreaded apartment) for the lifetime of the
	// object. Cheap to nest; only the outermost scope on a given thread
	// actually does anything observable since CoInitializeEx ref-counts.
	struct ComScope {
		bool ok;
		ComScope() : ok(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {}
		~ComScope() { if (ok) CoUninitialize(); }
	};

	// Parses a UTF-8 XML document. Returns null on failure (does not throw).
	CComPtr<IXMLDOMDocument> Load(const char* utf8xml);

	// Returns the local name of a node (the part after any "prefix:").
	pfc::string8 LocalName(IXMLDOMNode* node);

	// First immediate child element whose local name matches (case-insensitive).
	CComPtr<IXMLDOMNode> ChildByName(IXMLDOMNode* parent, const wchar_t* name);

	// All immediate children whose local name matches.
	void ChildrenByName(IXMLDOMNode* parent, const wchar_t* name, std::vector<CComPtr<IXMLDOMNode>>& out);

	// Text content of a node, or empty string if null.
	pfc::string8 Text(IXMLDOMNode* node);

	// Text content of the first matching immediate child, or empty string.
	pfc::string8 ChildText(IXMLDOMNode* parent, const wchar_t* name);

	// Named attribute value, or empty string if absent.
	pfc::string8 Attr(IXMLDOMNode* node, const wchar_t* name);

}
