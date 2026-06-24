#include "stdafx.h"
#include "resource.h"
#include "config.h"
#include "panel_colors.h"
#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>
#include <shared/shared.h>

namespace {

	class CPodcastPreferences : public CDialogImpl<CPodcastPreferences>, public preferences_page_instance {
	public:
		CPodcastPreferences(preferences_page_callback::ptr callback) : m_callback(callback) {}

		enum { IDD = IDD_PODCAST_PREFERENCES };

		t_uint32 get_state();
		void apply();
		void reset();

		BEGIN_MSG_MAP_EX(CPodcastPreferences)
			MSG_WM_INITDIALOG(OnInitDialog)
			COMMAND_HANDLER_EX(IDC_COLOR_THEME, BN_CLICKED, OnEditChange)
			COMMAND_HANDLER_EX(IDC_COLOR_CUSTOM, BN_CLICKED, OnEditChange)
			COMMAND_ID_HANDLER_EX(IDC_BG_COLOR_BTN, OnBgColor)
			COMMAND_ID_HANDLER_EX(IDC_TEXT_COLOR_BTN, OnTextColor)
		END_MSG_MAP()

	private:
		BOOL OnInitDialog(CWindow, LPARAM) {
			m_dark.AddDialogWithControls(*this);
			m_mode = podcast_cfg::color_mode.get();
			m_bg = (COLORREF)podcast_cfg::custom_bg_color.get();
			m_text = (COLORREF)podcast_cfg::custom_text_color.get();
			CheckRadio();
			return TRUE;
		}

		void CheckRadio() {
			CheckDlgButton(IDC_COLOR_THEME, m_mode == podcast_cfg::color_mode_theme);
			CheckDlgButton(IDC_COLOR_CUSTOM, m_mode == podcast_cfg::color_mode_custom);
		}

		void OnEditChange(UINT, int, CWindow) {
			m_mode = IsDlgButtonChecked(IDC_COLOR_CUSTOM) ? podcast_cfg::color_mode_custom : podcast_cfg::color_mode_theme;
			OnChanged();
		}

		bool PickColor(COLORREF& inout) {
			static COLORREF customColors[16] = {};
			CHOOSECOLOR cc = {};
			cc.lStructSize = sizeof(cc);
			cc.hwndOwner = m_hWnd;
			cc.rgbResult = inout;
			cc.lpCustColors = customColors;
			cc.Flags = CC_FULLOPEN | CC_RGBINIT;
			if (!ChooseColor(&cc)) return false;
			inout = cc.rgbResult;
			return true;
		}

		void OnBgColor(UINT, int, CWindow) {
			if (PickColor(m_bg)) OnChanged();
		}
		void OnTextColor(UINT, int, CWindow) {
			if (PickColor(m_text)) OnChanged();
		}

		bool HasChanged() {
			return m_mode != podcast_cfg::color_mode.get()
				|| (t_uint32)m_bg != podcast_cfg::custom_bg_color.get()
				|| (t_uint32)m_text != podcast_cfg::custom_text_color.get();
		}
		void OnChanged() { m_callback->on_state_changed(); }

		const preferences_page_callback::ptr m_callback;
		fb2k::CDarkModeHooks m_dark;
		t_uint32 m_mode = podcast_cfg::color_mode_theme;
		COLORREF m_bg = RGB(255, 255, 255);
		COLORREF m_text = RGB(0, 0, 0);
	};

	t_uint32 CPodcastPreferences::get_state() {
		t_uint32 state = preferences_state::resettable;
		if (HasChanged()) state |= preferences_state::changed;
		return state;
	}

	void CPodcastPreferences::reset() {
		m_mode = podcast_cfg::color_mode_theme;
		m_bg = RGB(255, 255, 255);
		m_text = RGB(0, 0, 0);
		CheckRadio();
		OnChanged();
	}

	void CPodcastPreferences::apply() {
		podcast_cfg::color_mode = m_mode;
		podcast_cfg::custom_bg_color = (t_uint32)m_bg;
		podcast_cfg::custom_text_color = (t_uint32)m_text;
		RefreshAllPodcastPanelColors();
		OnChanged();
	}

	class preferences_page_podcast : public preferences_page_impl<CPodcastPreferences> {
	public:
		const char* get_name() { return "Podcasts"; }
		GUID get_guid() {
			static constexpr GUID guid = { 0x5c8d1e3a, 0x9f4b, 0x4a72, { 0x8c, 0x1d, 0x6e, 0x3a, 0x90, 0x2f, 0x55, 0x18 } };
			return guid;
		}
		GUID get_parent_guid() { return guid_tools; }
	};

	static preferences_page_factory_t<preferences_page_podcast> g_preferences_page_podcast_factory;
}
