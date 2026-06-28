#include "stdafx.h"
#include <windowsx.h>
#include <atlctrls.h>
#include <libPPUI/win32_op.h>
#include <helpers/BumpableElem.h>
#include <helpers/DarkMode.h>
#include <SDK/file_info_impl.h>
#include <vector>
#include <algorithm>
#include "model.h"
#include "library_service.h"
#include "input_dialog.h"
#include "config.h"
#include "panel_colors.h"

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
		ID_PLAY_ALL,
		ID_ADD_CHECKED_TO_PODCASTY,
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
			: m_callback(p_callback), m_config(config) {
			g_instances().push_back(this);
		}

		~CPodcastPanel() {
			auto& v = g_instances();
			v.erase(std::remove(v.begin(), v.end(), this), v.end());
		}

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
				ApplyColors();
				Invalidate();
			}
		}

		void ApplyColors() {
			if (!m_tree.IsWindow()) return;
			if (podcast_cfg::color_mode.get() == podcast_cfg::color_mode_custom) {
				COLORREF bg = (COLORREF)podcast_cfg::custom_bg_color.get();
				COLORREF text = (COLORREF)podcast_cfg::custom_text_color.get();
				// Match the tree's themed border/scrollbars to the custom background's
				// brightness, otherwise the native control keeps drawing a light-theme
				// (white) border around a dark custom background.
				double luma = 0.299 * GetRValue(bg) + 0.587 * GetGValue(bg) + 0.114 * GetBValue(bg);
				m_dark.SetDark(luma < 128.0);
				m_tree.SetBkColor(bg);
				m_tree.SetTextColor(text);
			} else {
				bool dark = m_callback.is_valid() && fb2k::isDarkMode();
				m_dark.SetDark(dark);
				m_tree.SetBkColor(m_callback->query_std_color(ui_color_background));
				m_tree.SetTextColor(m_callback->query_std_color(ui_color_text));
			}
			m_tree.Invalidate();
		}

		static std::vector<CPodcastPanel*>& g_instances() {
			static std::vector<CPodcastPanel*> v;
			return v;
		}

		BEGIN_MSG_MAP_EX(CPodcastPanel)
			MSG_WM_CREATE(OnCreate)
			MSG_WM_DESTROY(OnDestroy)
			MSG_WM_ERASEBKGND(OnEraseBkgnd)
			MSG_WM_SIZE(OnSize)
			MSG_WM_CONTEXTMENU(OnContextMenu)
			NOTIFY_CODE_HANDLER_EX(NM_DBLCLK, OnTreeDblClk)
			NOTIFY_CODE_HANDLER_EX(NM_CLICK, OnTreeClick)
			MESSAGE_HANDLER_EX(WM_USER + 1, OnCheckboxSettled)
		END_MSG_MAP()

	private:
		LRESULT OnCreate(LPCREATESTRUCT) {
			CRect rc; GetClientRect(&rc);
			m_tree.Create(m_hWnd, rc, nullptr,
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_CHECKBOXES,
				0, 1);
			m_dark.AddDialogWithControls(m_hWnd);
			ApplyColors();
			RebuildTree();
			return 0;
		}

		void OnDestroy() {
			ClearNodeData();
		}

		void OnSize(UINT, CSize size) {
			if (m_tree.IsWindow()) m_tree.MoveWindow(0, 0, size.cx, size.cy);
		}

		//! Paint the panel's own background with the active (themed or custom)
		//! background colour. Without this the default window brush shows through
		//! as a white frame around the tree control when foobar2000 is in dark mode.
		BOOL OnEraseBkgnd(CDCHandle dc) {
			CRect rc; GetClientRect(&rc);
			COLORREF bg;
			if (podcast_cfg::color_mode.get() == podcast_cfg::color_mode_custom) {
				bg = (COLORREF)podcast_cfg::custom_bg_color.get();
			} else {
				bg = m_callback.is_valid() ? m_callback->query_std_color(ui_color_background) : GetSysColor(COLOR_WINDOW);
			}
			dc.FillSolidRect(&rc, bg);
			return TRUE;
		}

		LRESULT OnTreeClick(LPNMHDR) {
			DWORD pos = GetMessagePos();
			CPoint pt(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));
			m_tree.ScreenToClient(&pt);
			UINT flags = 0;
			HTREEITEM hit = m_tree.HitTest(pt, &flags);
			if (hit && (flags & TVHT_ONITEMSTATEICON)) {
				NodeData* node = (NodeData*)m_tree.GetItemData(hit);
				if (node && !node->isChannel) PostMessage(WM_USER + 1, 0, (LPARAM)hit);
			}
			return 0;
		}

		LRESULT OnCheckboxSettled(UINT, WPARAM, LPARAM lParam) {
			UpdateEpisodeLabel((HTREEITEM)lParam);
			return 0;
		}

		LRESULT OnTreeDblClk(LPNMHDR) {
			NodeData* node = GetSelectedNode();
			if (node && node->isChannel) PlayChannelAll();
			else PlaySelected();
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
				m_tree.SetCheckState(chItem, FALSE);
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
					pfc::stringcvt::string_wide_from_utf8 epLabelW(BuildEpisodeLabel(ep, false).get_ptr());
					HTREEITEM epItem = m_tree.InsertItem(epLabelW.get_ptr(), chItem, TVI_LAST);
					NodeData* epData = new NodeData{ false, chIdx, epIdx };
					m_nodeData.push_back(epData);
					m_tree.SetItemData(epItem, (DWORD_PTR)epData);
					m_tree.SetCheckState(epItem, FALSE);
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

		//! Builds an episode's tree label. The "queued" marker is plain text rather
		//! than relying on the native checkbox glyph, which can be hard to see
		//! against custom panel colors.
		static pfc::string8 BuildEpisodeLabel(const podcast::Episode& ep, bool checked) {
			pfc::string8 label = checked ? "[+] " : (ep.listened ? "[x] " : "[ ] ");
			pfc::string8 date = FormatDate(ep.pubDate);
			if (!date.is_empty()) { label << date << "  "; }
			label << ep.title;
			return label;
		}

		void UpdateEpisodeLabel(HTREEITEM epItem) {
			NodeData* node = (NodeData*)m_tree.GetItemData(epItem);
			if (!node || node->isChannel) return;
			podcast::Library& lib = podcast::GetLibrary();
			if (node->channelIndex >= lib.channels.get_count()) return;
			podcast::Channel& ch = lib.channels[node->channelIndex];
			if (node->episodeIndex >= ch.episodes.get_count()) return;
			const podcast::Episode& ep = ch.episodes[node->episodeIndex];
			bool checked = m_tree.GetCheckState(epItem) != 0;
			pfc::stringcvt::string_wide_from_utf8 labelW(BuildEpisodeLabel(ep, checked).get_ptr());
			m_tree.SetItemText(epItem, labelW.get_ptr());
		}

		static t_size GetOrCreatePlaylist(const char* name) {
			static_api_ptr_t<playlist_manager> pm;
			return pm->find_or_create_playlist(name, strlen(name));
		}

		//! Builds metadb handles for the given episodes and forces their displayed
		//! title (and album/length) to the feed's own data, so playlists show the
		//! episode title instead of a filename guessed from the stream URL.
		static metadb_handle_list BuildEpisodeHandles(const std::vector<const podcast::Episode*>& eps, const char* channelTitle) {
			static_api_ptr_t<metadb> mdb;
			static_api_ptr_t<metadb_io_v2> io;
			metadb_handle_list handles;
			metadb_hint_list::ptr hints = io->create_hint_list();
			for (const podcast::Episode* ep : eps) {
				if (ep->url.is_empty()) continue;
				metadb_handle_ptr h = mdb->handle_create(make_playable_location(ep->url.get_ptr(), 0));
				file_info_impl info;
				info.meta_set("TITLE", ep->title.is_empty() ? "(untitled episode)" : ep->title.get_ptr());
				if (channelTitle && *channelTitle) info.meta_set("ALBUM", channelTitle);
				if (ep->durationSeconds > 0) info.set_length((double)ep->durationSeconds);
				hints->add_hint(h, info, filestats_invalid, true);
				handles.add_item(h);
			}
			hints->on_done();
			return handles;
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

			std::vector<const podcast::Episode*> eps{ &ep };
			metadb_handle_list handles = BuildEpisodeHandles(eps, ch.title.get_ptr());
			if (handles.get_count() == 0) return;

			static_api_ptr_t<playlist_manager> pm;
			t_size playlistIdx = GetOrCreatePlaylist("Podcasty");
			pm->set_active_playlist(playlistIdx);
			t_size newItemIdx = pm->playlist_get_item_count(playlistIdx);
			pm->playlist_add_items(playlistIdx, handles, pfc::bit_array_false());
			pm->playlist_execute_default_action(playlistIdx, newItemIdx);
		}

		void PlayChannelAll() {
			NodeData* node = GetSelectedNode();
			if (!node || !node->isChannel) return;
			podcast::Library& lib = podcast::GetLibrary();
			if (node->channelIndex >= lib.channels.get_count()) return;
			podcast::Channel& ch = lib.channels[node->channelIndex];
			if (ch.episodes.get_count() == 0) return;

			std::vector<const podcast::Episode*> eps;
			for (t_size i = 0; i < ch.episodes.get_count(); ++i) eps.push_back(&ch.episodes[i]);
			metadb_handle_list handles = BuildEpisodeHandles(eps, ch.title.get_ptr());
			if (handles.get_count() == 0) return;

			static_api_ptr_t<playlist_manager> pm;
			t_size playlistIdx = GetOrCreatePlaylist(ch.title.is_empty() ? "Podcasty" : ch.title.get_ptr());
			pm->set_active_playlist(playlistIdx);
			t_size firstNewIdx = pm->playlist_get_item_count(playlistIdx);
			pm->playlist_add_items(playlistIdx, handles, pfc::bit_array_false());
			pm->playlist_execute_default_action(playlistIdx, firstNewIdx);
		}

		void AddCheckedEpisodesToPodcasty() {
			podcast::Library& lib = podcast::GetLibrary();
			std::vector<const podcast::Episode*> eps;
			std::vector<HTREEITEM> checkedItems;

			for (HTREEITEM chItem = m_tree.GetRootItem(); chItem; chItem = m_tree.GetNextSiblingItem(chItem)) {
				for (HTREEITEM epItem = m_tree.GetChildItem(chItem); epItem; epItem = m_tree.GetNextSiblingItem(epItem)) {
					if (!m_tree.GetCheckState(epItem)) continue;
					NodeData* node = (NodeData*)m_tree.GetItemData(epItem);
					if (!node || node->isChannel) continue;
					if (node->channelIndex >= lib.channels.get_count()) continue;
					podcast::Channel& ch = lib.channels[node->channelIndex];
					if (node->episodeIndex >= ch.episodes.get_count()) continue;
					const podcast::Episode& ep = ch.episodes[node->episodeIndex];
					if (!ep.url.is_empty()) eps.push_back(&ep);
					checkedItems.push_back(epItem);
				}
			}

			if (eps.empty()) {
				popup_message::g_show("Check at least one episode first (click its checkbox), then use this command to add it to the \"Podcasty\" playlist.", "Podcasts");
				return;
			}

			metadb_handle_list handles = BuildEpisodeHandles(eps, nullptr);

			static_api_ptr_t<playlist_manager> pm;
			t_size playlistIdx = GetOrCreatePlaylist("Podcasty");
			pm->set_active_playlist(playlistIdx);
			pm->playlist_add_items(playlistIdx, handles, pfc::bit_array_false());

			for (HTREEITEM item : checkedItems) {
				m_tree.SetCheckState(item, FALSE);
				UpdateEpisodeLabel(item);
			}
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
				menu.AppendMenu(MF_STRING, ID_PLAY_ALL, _T("Play all episodes"));
				menu.AppendMenu(MF_STRING, ID_REFRESH_CHANNEL, _T("Refresh this podcast"));
				menu.AppendMenu(MF_STRING, ID_REMOVE_CHANNEL, _T("Remove podcast"));
				menu.AppendMenu(MF_SEPARATOR);
			}

			menu.AppendMenu(MF_STRING, ID_ADD_CHECKED_TO_PODCASTY, _T("Add checked episodes to \"Podcasty\" playlist"));
			menu.AppendMenu(MF_SEPARATOR);

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
			case ID_PLAY_ALL:
				PlayChannelAll();
				break;
			case ID_ADD_CHECKED_TO_PODCASTY:
				AddCheckedEpisodesToPodcasty();
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
		fb2k::CDarkModeHooks m_dark;
	protected:
		const ui_element_instance_callback_ptr m_callback;
	};

	class ui_element_podcast_impl : public ui_element_impl_withpopup<CPodcastPanel> {};
	static service_factory_single_t<ui_element_podcast_impl> g_ui_element_podcast_impl_factory;

}

void RefreshAllPodcastPanelColors() {
	for (auto* p : CPodcastPanel::g_instances()) {
		p->ApplyColors();
	}
}
