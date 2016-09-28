#include "mxAUI.h"
#include "Language.h"
#include "mxHidenPanel.h"

//AuiSettings::AuiSettings() {
//	m_panes.Resize(auiPanes::Count);
//	{
//		PaneConfig &pi = m_panes[auiPanes::Compiler];
//		pi.key = "compiler";
//		pi.shortname = LANG(CAPTION_COMPILER_OUTPUT,"Resultados de la Compilación");
//		pi.caption = "";
//		pi.autohide = false; 
//		pi.margin = DockBottom;
//		pi.level = 0;
//		pi.order = 0;
//	}
//	{	
//		PaneConfig &pi = m_panes[auiPanes::QuickHelp];
//		pi.key = "quickhelp";
//		pi.shortname = LANG(CAPTION_QUIKHELP,"Búsqueda/Ayuda Rapida");
//		pi.caption = "";
//		pi.autohide = false; 
//		pi.margin = DockBottom;
//		pi.level = 0;
//		pi.order = 1;
//	}
//	{
//		PaneConfig &pi = m_panes[auiPanes::DebugMsgs];
//		pi.key = "debugmsg";
//		pi.shortname = "";
//		pi.caption = "";
//		pi.autohide = false; 
//		pi.margin = DockBottom;
//		pi.level = 0;
//		pi.order = 2;
//	}
//	{
//		PaneConfig &pi = m_panes[auiPanes::Threads];
//		pi.key = "threads"; 
//		pi.shortname = "";
//		pi.caption = "";
//		pi.autohide = false; 
//		pi.margin = DockBottom;
//		pi.level = 0;
//		pi.order = 3;
//	}
//	{
//		PaneConfig &pi = m_panes[auiPanes::Backtrace];
//		pi.key = "backtrace";
//		pi.shortname = LANG(MAINW_AUTOHIDE_BACKTRACE,"Trazado Inverso");
//		pi.caption = LANG(CAPTION_BACKTRACE,"Trazado Inverso");
//		pi.autohide = false; 
//		pi.margin = DockBottom;
//		pi.level = 0;
//		pi.order = 4;
//	}
//	{
//		PaneConfig &pi = m_panes[auiPanes::Inspections];
//		pi.key = "inspections";
//		pi.shortname = "";
//		pi.caption = "";
//		pi.autohide = false; 
//		pi.margin = DockBottom;
//		pi.level = 0;
//		pi.order = 5;
//	}
//	{
//		PaneConfig &pi = m_panes[auiPanes::Project];
//		pi.key = "project";
//		pi.shortname = "";
//		pi.caption = "";
//		pi.autohide = false; 
//		pi.margin = DockLeft;
//		pi.level = 0;
//		pi.order = 0;
//	}
//	{
//		PaneConfig &pi = m_panes[auiPanes::Symbols];
//		pi.key = "symbols";
//		pi.shortname = "";
//		pi.caption = "";
//		pi.autohide = false; 
//		pi.margin = DockLeft;
//		pi.level = 0;
//		pi.order = 1;
//	}
//	{
//		PaneConfig &pi = m_panes[auiPanes::Explorer];
//		pi.key = "explorer";
//		pi.shortname = "";
//		pi.caption = "";
//		pi.autohide = false; 
//		pi.margin = DockLeft;
//		pi.level = 0;
//		pi.order = 2;
//	}
//	{
//		PaneConfig &pi = m_panes[auiPanes::Tools];
//		pi.key = "tools";
//		pi.shortname = "";
//		pi.caption = "";
//		pi.autohide = false; 
//		pi.margin = DockBottom;
//		pi.level = 1;
//		pi.order = 0;
//	}
//	{
//		PaneConfig &pi = m_panes[auiPanes::Extern];
//		pi.key = "extern";
//		pi.shortname = "";
//		pi.caption = "";
//		pi.autohide = false; 
//		pi.margin = DockBottom;
//		pi.level = 0;
//		pi.order = 0;
//	}
//	{
//		PaneConfig &pi = m_panes[auiPanes::Registers];
//		pi.key = "registers";
//		pi.shortname = "";
//		pi.caption = "";
//		pi.autohide = false; 
//		pi.margin = DockRight;
//		pi.level = 0;
//		pi.order = 0;
//	}
//	{
//		PaneConfig &pi = m_panes[auiPanes::Templates];
//		pi.key = "templates";
//		pi.shortname = "";
//		pi.caption = "";
//		pi.autohide = false; 
//		pi.margin = DockRight;
//		pi.level = 0;
//		pi.order = 1;
//	}
//}
//
//void AuiSettings::ParseConfigLine (const wxString & key, const wxString & value) {
//	
//}
//
//void AuiSettings::WriteConfig (wxTextFile & file) {
//	
//}

mxAUI::mxAUI (wxWindow *parent) : m_wxaui(*this), m_freeze_level(0) {
	m_wxaui.SetManagedWindow(parent);
//	LoadDefaults();
}

//void mxAUI::Freeze ( ) {
//	m_freeze_level++;
//}
//
//void mxAUI::Thaw ( ) {
//	if (--m_freeze_level==0)
//		m_wxaui.Update();
//}

void mxAUI::Update ( ) {
	if (!m_freeze_level) m_wxaui.Update();
}

void mxAUI::OnPaneClose (wxWindow * window) {
	int iaux = m_panes.Find(window);
	if (iaux!=m_panes.NotFound()) {
		m_wxaui.DetachPane(window);
		if (m_panes[iaux].delete_on_close) window->Destroy();
		m_panes.Remove(iaux);
	}
}

void mxAUI::AttachGenericPane(wxWindow *ctrl, wxString title, wxPoint position, wxSize size, bool handle_deletion) {
	wxAuiPaneInfo pane_info;
	pane_info.Float().CloseButton(true).MaximizeButton(true).Resizable(true).Caption(title).Show();
	if (position!=wxDefaultPosition) pane_info.FloatingPosition(position);
	if (size!=wxDefaultSize) pane_info.BestSize(size);
	m_wxaui.AddPane(ctrl,pane_info);
	m_wxaui.Update();
	if (handle_deletion) m_panes.Add(mxPaneInfo(ctrl).DeleteOnClose());
}


//void mxAUI::AttachKnownPane(int aui_id, wxWindow *ctrl) {
//	AuiSettings::PaneConfig &psettings = m_settings.Get(aui_id);
//	wxAuiPaneInfo wxpi;
//	wxpi.Name(psettings.key).Caption(psettings.caption).CloseButton(true).MaximizeButton(true).Hide()
//		.Position(psettings.order).MaximizeButton(!psettings.autohide);
//	switch(psettings.margin) {
//		case AuiSettings::DockBottom: wxpi.Bottom(); break;
//		case AuiSettings::DockLeft: wxpi.Left(); break;
//		case AuiSettings::DockRight: wxpi.Right(); break;
//		case AuiSettings::Floating: wxpi.Float(); break;
//	};
//	m_wxaui.AddPane(ctrl,wxpi);
//	if (psettings.autohide) {
//		mxHidenPanel *helper = new mxHidenPanel(m_wxaui.GetManagedWindow(),ctrl,(hp_pos)psettings.margin,psettings.shortname);
//		m_panes.Add(mxPaneInfo(ctrl).HiddenHelper(helper));
//	}
//}
//
