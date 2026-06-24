#include "stdafx.h"
#include "resource.h"
#include "input_dialog.h"
#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>
#include <shared/shared.h>

namespace {
	class CPodcastInputDialog : public CDialogImpl<CPodcastInputDialog> {
	public:
		enum { IDD = IDD_PODCAST_INPUT };

		CPodcastInputDialog(const char* title, const char* prompt, const char* initialValue)
			: m_title(title), m_prompt(prompt), m_initialValue(initialValue) {}

		pfc::string8 m_result;
		bool m_ok = false;

		BEGIN_MSG_MAP_EX(CPodcastInputDialog)
			MSG_WM_INITDIALOG(OnInitDialog)
			COMMAND_ID_HANDLER_EX(IDOK, OnOK)
			COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
		END_MSG_MAP()

	private:
		BOOL OnInitDialog(CWindow, LPARAM) {
			m_dark.AddDialogWithControls(*this);
			uSetWindowText(m_hWnd, m_title.get_ptr());
			uSetDlgItemText(m_hWnd, IDC_INPUT_PROMPT, m_prompt.get_ptr());
			uSetDlgItemText(m_hWnd, IDC_INPUT_EDIT, m_initialValue.get_ptr());
			CenterWindow();
			return TRUE;
		}
		void OnOK(UINT, int, CWindow) {
			pfc::string8 buf;
			uGetDlgItemText(m_hWnd, IDC_INPUT_EDIT, buf);
			m_result = buf;
			m_ok = true;
			EndDialog(IDOK);
		}
		void OnCancel(UINT, int, CWindow) {
			EndDialog(IDCANCEL);
		}

		pfc::string8 m_title, m_prompt, m_initialValue;
		fb2k::CDarkModeHooks m_dark;
	};
}

bool ShowPodcastInputDialog(HWND parent, const char* title, const char* prompt, const char* initialValue, pfc::string8& p_value) {
	CPodcastInputDialog dlg(title, prompt, initialValue);
	dlg.DoModal(parent);
	if (dlg.m_ok) {
		p_value = dlg.m_result;
		return true;
	}
	return false;
}
