#include "stdafx.h"
#include <atlctrls.h>
#include <libPPUI/win32_op.h>
#include <helpers/BumpableElem.h>
#include <vector>
#include <algorithm>
#include "model.h"
#include "library_service.h"
#include "input_dialog.h"

namespace {

	static const GUID guid_podcast_panel = { 0x4f1a6d2c, 0x9b3e, 0x4c71, { 0x8a, 0x2d, 0x5e, 0x7c, 0x90, 0x1b, 0x33, 0x44 } };

	enum SortMode { SortDateDesc = 0, SortDateAsc = 1, SortName = 2 };

	enum {
		ID_ADD_URL = 1,
		ID_IMPORT_OPML,
		ID_EXPORT_OPML,
		ID_REFRESH_ALL,
		ID_REFRESH_CHANNEL,
		ID_REMOVE_CHANNEL,
		ID_PLAY,
		ID_TOGGLE_LISTENED,
		ID_SORT_DATE_DESC,
		ID_SORT_DATE_ASC,
		ID_SORT_NAME,
	};

	struct NodeData {
		bool isChannel;
		t_size channelIndex;
		t_size episodeIndex;
	};

	pfc::string8 FormatDate(time_t t) {
		if (t == 0) return pfc::string8("");
		struct tm tmv;
		if (gmtime_s(&tmv, &t) != 0) return pfc::string8("");
		char buf[32];
		sprintf_s(buf, "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
		return pfc::string8(buf);
	}

	class CPodcastPanel : public ui_element_instance, public CWindowImpl<CPodcastPanel> {
	public:
		DECLARE_WND_CLASS_EX(TEXT("{6B2C9A4E-1F3D-4E8A-9C5B-7D2A8F114455}"), CS_DBLCLKS, (-1));

		CPodcastPanel(ui_element_config::ptr config, ui_element_instance_callback_ptr p_callback)
			: m_callback(p_callback), m_config(config) {}

		void initialize_window(HWND parent) { WIN32_OP(Create(parent) != NULL); }
		HWND get_wnd() { return *this; }
		void set_configuration(ui_element_config::ptr config) { m_config = config; }
		ui_element_config::ptr get_configuration() { return m_config; }

		static GUID g_get_guid() { return guid_podcast_panel; }
		static GUID g_get_subclass() { return ui_element_subclass_utility; }
		static void g_get_name(pfc::string_base& out) { out = "Podcasts"; }
		static ui_element_config::ptr g_get_default_configuration() { return ui_element_config::g_create_empty(g_get_guid()); }
		static const char* g_get_description() { return "Podcast subscription library: import/export OPML, add feeds by URL, play and track listened episodes."; }
		void notify(const GUID& p_what, t_size, const void*, t_size) {
			if (p_what == ui_element_notify_colors_changed || p_what == ui_element_notify_font_changed) {
				Invalidate();
			}
		}

		BEGIN_MSG_MAP_EX(CPodcastPanel)
			MSG_WM_CREATE(OnCreate)
			MSG_WM_DESTROY(OnDestroy)
			MSG_WM_SIZE(OnSize)
			MSG_WM_CONTEXTMENU(OnContextMenu)
			NOTIFY_CODE_HANDLER_EX(NM_DBLCLK, OnTreeDblClk)
		END_MSG_MAP()

	private:
		LRESULT OnCreate(LPCREATESTRUCT) {
			CRect rc; GetClientRect(&rc);
			m_tree.Create(m_hWnd, rc, nullptr,
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
				0, 1);
			RebuildTree();
			return 0;
		}

		void OnDestroy() {
			ClearNodeData();
		}

		void OnSize(UINT, CSize size) {
			if (m_tree.IsWindow()) m_tree.MoveWindow(0, 0, size.cx, size.cy);
		}

		LRESULT OnTreeDblClk(LPNMHDR) {
			PlaySelected();
			return 0;
		}

		void ClearNodeData() {
			for (auto* p : m_nodeData) delete p;
			m_nodeData.clear();
		}

		void RebuildTree() {
			m_tree.SetRedraw(FALSE);
			m_tree.DeleteAllItems();
			ClearNodeData();

			podcast::Library& lib = podcast::GetLibrary();

			std::vector<t_size> chOrder;
			for (t_size i = 0; i < lib.channels.get_count(); ++i) chOrder.push_back(i);
			std::sort(chOrder.begin(), chOrder.end(), [&](t_size a, t_size b) {
				return _stricmp(lib.channels[a].title.get_ptr(), lib.channels[b].title.get_ptr()) < 0;
			});

			for (t_size chIdx : chOrder) {
				const podcast::Channel& ch = lib.channels[chIdx];

				t_size unlistened = 0;
				for (t_size i = 0; i < ch.episodes.get_count(); ++i) if (!ch.episodes[i].listened) ++unlistened;

				pfc::string8 label = ch.title;
				if (unlistened > 0) { label << " ("; label << pfc::format_int(unlistened); label << ")"; }

				pfc::stringcvt::string_wide_from_utf8 labelW(label.get_ptr());
				HTREEITEM chItem = m_tree.InsertItem(labelW.get_ptr(), TVI_ROOT, TVI_LAST);
				NodeData* chData = new NodeData{ true, chIdx, 0 };
				m_nodeData.push_back(chData);
				m_tree.SetItemData(chItem, (DWORD_PTR)chData);

				std::vector<t_size> epOrder;
				for (t_size i = 0; i < ch.episodes.get_count(); ++i) epOrder.push_back(i);
				std::stable_sort(epOrder.begin(), epOrder.end(), [&](t_size a, t_size b) {
					const podcast::Episode& ea = ch.episodes[a];
					const podcast::Episode& eb = ch.episodes[b];
					switch (m_sortMode) {
					case SortDateAsc: return ea.pubDate < eb.pubDate;
					case SortName: return _stricmp(ea.title.get_ptr(), eb.title.get_ptr()) < 0;
					default: return ea.pubDate > eb.pubDate;
					}
				});

				for (t_size epIdx : epOrder) {
					const podcast::Episode& ep = ch.episodes[epIdx];
					pfc::string8 epLabel = ep.listened ? "[x] " : "[ ] ";
					pfc::string8 date = FormatDate(ep.pubDate);
					if (!date.is_empty()) { epLabel << date << "  "; }
					epLabel << ep.title;

					pfc::stringcvt::string_wide_from_utf8 epLabelW(epLabel.get_ptr());
					HTREEITEM epItem = m_tree.InsertItem(epLabelW.get_ptr(), chItem, TVI_LAST);
					NodeData* epData = new NodeData{ false, chIdx, epIdx };
					m_nodeData.push_back(epData);
					m_tree.SetItemData(epItem, (DWORD_PTR)epData);
				}

				m_tree.Expand(chItem, TVE_EXPAND);
			}

			m_tree.SetRedraw(TRUE);
			m_tree.Invalidate();
		}

		NodeData* GetSelectedNode() {
			HTREEITEM sel = m_tree.GetSelectedItem();
			if (!sel) return nullptr;
			return (NodeData*)m_tree.GetItemData(sel);
		}

		void PlaySelected() {
			NodeData* node = GetSelectedNode();
			if (!node || node->isChannel) return;
			podcast::Library& lib = podcast::GetLibrary();
			if (node->channelIndex >= lib.channels.get_count()) return;
			podcast::Channel& ch = lib.channels[node->channelIndex];
			if (node->episodeIndex >= ch.episodes.get_count()) return;
			const podcast::Episode& ep = ch.episodes[node->episodeIndex];
			if (ep.url.is_empty()) return;

			static_api_ptr_t<playlist_manager> pm;
			t_size idx = pm->activeplaylist_get_item_count();
			pfc::list_t<const char*> paths;
			paths.add_item(ep.url.get_ptr());
			pm->activeplaylist_add_locations(paths, false, core_api::get_main_window());
			pm->activeplaylist_execute_default_action(idx);
		}

		void ToggleListenedSelected() {
			NodeData* node = GetSelectedNode();
			if (!node || node->isChannel) return;
			podcast::Library& lib = podcast::GetLibrary();
			if (node->channelIndex >= lib.channels.get_count()) return;
			podcast::Channel& ch = lib.channels[node->channelIndex];
			if (node->episodeIndex >= ch.episodes.get_count()) return;
			ch.episodes[node->episodeIndex].listened = !ch.episodes[node->episodeIndex].listened;
			lib.save();
			RebuildTree();
		}

		void RemoveSelectedChannel() {
			NodeData* node = GetSelectedNode();
			if (!node || !node->isChannel) return;
			podcast::Library& lib = podcast::GetLibrary();
			lib.remove_channel(node->channelIndex);
			lib.save();
			RebuildTree();
		}

		pfc::string8 BrowseFile(bool save) {
			wchar_t buffer[MAX_PATH] = {};
			OPENFILENAMEW ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = m_hWnd;
			ofn.lpstrFilter = L"OPML files\0*.opml;*.xml\0All files\0*.*\0";
			ofn.lpstrFile = buffer;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrDefExt = L"opml";
			BOOL ok;
			if (save) {
				ok = GetSaveFileNameW(&ofn);
			} else {
				ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
				ok = GetOpenFileNameW(&ofn);
			}
			if (!ok) return pfc::string8();
			return pfc::string8(pfc::stringcvt::string_utf8_from_wide(buffer).get_ptr());
		}

		void OnContextMenu(CWindow, CPoint pt) {
			if (pt.x != -1 || pt.y != -1) {
				CPoint client = pt;
				m_tree.ScreenToClient(&client);
				UINT flags = 0;
				HTREEITEM hit = m_tree.HitTest(client, &flags);
				if (hit) m_tree.SelectItem(hit);
			}

			NodeData* node = GetSelectedNode();

			CMenu menu;
			menu.CreatePopupMenu();

			if (node && !node->isChannel) {
				menu.AppendMenu(MF_STRING, ID_PLAY, _T("Play"));
				podcast::Library& lib = podcast::GetLibrary();
				bool listened = false;
				if (node->channelIndex < lib.channels.get_count()) {
					const podcast::Channel& ch = lib.channels[node->channelIndex];
					if (node->episodeIndex < ch.episodes.get_count()) listened = ch.episodes[node->episodeIndex].listened;
				}
				menu.AppendMenu(MF_STRING, ID_TOGGLE_LISTENED, listened ? _T("Mark as unlistened") : _T("Mark as listened"));
				menu.AppendMenu(MF_SEPARATOR);
			}
			if (node && node->isChannel) {
				menu.AppendMenu(MF_STRING, ID_REFRESH_CHANNEL, _T("Refresh this podcast"));
				menu.AppendMenu(MF_STRING, ID_REMOVE_CHANNEL, _T("Remove podcast"));
				menu.AppendMenu(MF_SEPARATOR);
			}

			CMenu sortMenu;
			sortMenu.CreatePopupMenu();
			sortMenu.AppendMenu(MF_STRING | (m_sortMode == SortDateDesc ? MF_CHECKED : 0), ID_SORT_DATE_DESC, _T("Newest first"));
			sortMenu.AppendMenu(MF_STRING | (m_sortMode == SortDateAsc ? MF_CHECKED : 0), ID_SORT_DATE_ASC, _T("Oldest first"));
			sortMenu.AppendMenu(MF_STRING | (m_sortMode == SortName ? MF_CHECKED : 0), ID_SORT_NAME, _T("Name"));
			menu.AppendMenu(MF_POPUP, sortMenu.m_hMenu, _T("Sort episodes by"));
			menu.AppendMenu(MF_SEPARATOR);

			menu.AppendMenu(MF_STRING, ID_ADD_URL, _T("Add podcast by URL..."));
			menu.AppendMenu(MF_STRING, ID_IMPORT_OPML, _T("Import OPML..."));
			menu.AppendMenu(MF_STRING, ID_EXPORT_OPML, _T("Export OPML..."));
			menu.AppendMenu(MF_SEPARATOR);
			menu.AppendMenu(MF_STRING, ID_REFRESH_ALL, _T("Refresh all podcasts"));

			int cmd = menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, m_hWnd);
			HandleCommand(cmd, node);
		}

		void HandleCommand(int cmd, NodeData* node) {
			HWND parent = core_api::get_main_window();
			switch (cmd) {
			case ID_PLAY:
				PlaySelected();
				break;
			case ID_TOGGLE_LISTENED:
				ToggleListenedSelected();
				break;
			case ID_REFRESH_CHANNEL:
				if (node && node->isChannel) {
					library_service::RefreshChannel(node->channelIndex, parent, [this] { RebuildTree(); });
				}
				break;
			case ID_REMOVE_CHANNEL:
				RemoveSelectedChannel();
				break;
			case ID_SORT_DATE_DESC: m_sortMode = SortDateDesc; RebuildTree(); break;
			case ID_SORT_DATE_ASC: m_sortMode = SortDateAsc; RebuildTree(); break;
			case ID_SORT_NAME: m_sortMode = SortName; RebuildTree(); break;
			case ID_ADD_URL: {
				pfc::string8 value;
				if (ShowPodcastInputDialog(parent, "Podcasts", "Podcast RSS/Atom feed URL:", "", value) && !value.is_empty()) {
					library_service::AddPodcastByUrl(value.get_ptr(), parent, [this] { RebuildTree(); });
				}
				break;
			}
			case ID_IMPORT_OPML: {
				pfc::string8 path = BrowseFile(false);
				if (!path.is_empty()) {
					library_service::ImportOpmlFile(path.get_ptr(), parent, [this] { RebuildTree(); });
				}
				break;
			}
			case ID_EXPORT_OPML: {
				pfc::string8 path = BrowseFile(true);
				if (!path.is_empty()) {
					if (!library_service::ExportOpmlFile(path.get_ptr())) {
						popup_message::g_show("Could not write the OPML file.", "Podcasts: export failed");
					}
				}
				break;
			}
			case ID_REFRESH_ALL:
				library_service::RefreshAll(parent, [this] { RebuildTree(); });
				break;
			default:
				break;
			}
		}

		CTreeViewCtrl m_tree;
		SortMode m_sortMode = SortDateDesc;
		std::vector<NodeData*> m_nodeData;

		ui_element_config::ptr m_config;
	protected:
		const ui_element_instance_callback_ptr m_callback;
	};

	class ui_element_podcast_impl : public ui_element_impl_withpopup<CPodcastPanel> {};
	static service_factory_single_t<ui_element_podcast_impl> g_ui_element_podcast_impl_factory;

}
