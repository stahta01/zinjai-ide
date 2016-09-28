#ifndef MXAUI_H
#define MXAUI_H
#include <wx/aui/aui.h>
#include <map>
#include <wx/textfile.h>
#include "SingleList.h"
#include "Cpp11.h"

//struct auiPanes {
//	enum { Compiler, QuickHelp, DebugMsgs, Threads, Backtrace, Inspections, Templates, Project, Symbols, Explorer, Tools, Extern, Registers, Count };
//};

//class AuiSettings {
//public:
//	enum { DockLeft, DockBottom, DockRight, DockTop, Floating };
//	enum { ShowAndRestore, ShowAndKeep, ShowAndHide, HideAndShow, HideAndRestore, HideAndKeep, Nothing };
//	enum { Hidden, Shown, RememberLast, NotAvailable };
//	struct PaneConfig {
//		wxString key, shortname, caption;
//		bool autohide;
//		int margin, level, order;
//		int debug_behaviour, fullscreen_behaviour, init_mode;
//	};
//	SingleList<PaneConfig> m_panes;
//	AuiSettings();
//	void ParseConfigLine(const wxString &key, const wxString &value);
//	void WriteConfig(wxTextFile &file);
//	PaneConfig &Get(int id) { return m_panes[id]; }
//};
	
class mxHidenPanel;

class mxAUI : public wxAuiManager {
//	AuiSettings m_settings;
	wxAuiManager &m_wxaui;
	int m_freeze_level;
	struct mxPaneInfo {
		wxWindow *window;
		bool delete_on_close;
		mxHidenPanel *hidden_helper;
		mxPaneInfo() : window(nullptr) {}
		mxPaneInfo(wxWindow *win) : window(win), delete_on_close(false), hidden_helper(nullptr) {}
		bool operator==(const mxPaneInfo &other) const { return window==other.window; }
		mxPaneInfo &DeleteOnClose() { delete_on_close = true; return *this; }
		mxPaneInfo &HiddenHelper(mxHidenPanel *helper) { hidden_helper = helper; return *this; }
	};
	SingleList<mxPaneInfo> m_panes;
public:
	mxAUI(wxWindow *parent);
	void OnPaneClose(wxWindow *window);
	void Update();
//	void Freeze();
//	void Thaw();
//	void AttachKnownPane(int aui_id, wxWindow *ctrl);
	void AttachGenericPane(wxWindow *ctrl, wxString title, wxPoint position, wxSize size, bool handle_deletion=true);
};

#endif

