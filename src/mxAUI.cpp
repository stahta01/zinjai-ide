#include "mxAUI.h"
#include "Language.h"
#include "mxHidenPanel.h"
#include "asserts.h"
#include "ConfigManager.h"
#include "mxAUIContainer.h"
#include "mxMainWindow.h"
#include "MenusAndToolsConfig.h"

#warning TODO: VER EL ONPANECLOSE DE MAINWINDOW

#define MAX_LAYER 10
#include <vector>
#include <set>

PaneConfig PaneConfig::m_configs[PaneId::Count];
void PaneConfig::Init ( ) {
//	for(int i=0;i<PaneId::Count;i++) m_configs[i].m_id = (PaneId::type)i;
	
	m_configs[PaneId::Compiler   ].Docked(Bottom).Layout(2,0).Actions(None,None,Hide).Titles(LANG(CAPTION_COMPILER_OUTPUT,"Resultados de la Compilación"),LANG(MAINW_AUTOHIDE_COMPILER,"Compilador")).MenuItem(mxID_VIEW_COMPILER_TREE);
	m_configs[PaneId::QuickHelp  ].Docked(Bottom).Layout(2,2).Actions(None,None,Hide).Titles(LANG(CAPTION_QUIKHELP,"Búsqueda/Ayuda Rapida"),LANG(MAINW_AUTOHIDE_QUICKHELP,"Ayuda/Busqueda"));
	m_configs[PaneId::DebugMsgs  ].Docked(Bottom).Layout(2,3).Actions(Hide,Hide,None).Titles(LANG(CAPTION_DEBUGGER_LOG,"Mensajes del Depurador"),LANG(MAINW_AUTOHIDE_DEBUG_LOG,"Log Depurador"))/*.MenuItem(mxID_DEBUG_LOG_PANEL)*/;
	m_configs[PaneId::Threads    ].Docked(Bottom).Layout(2,4).Actions(Hide,Hide,None).Titles(LANG(CAPTION_THREADLIST,"Hilos de Ejecución"),LANG(MAINW_AUTOHIDE_THREADS,"Hilos"))/*.MenuItem(mxID_DEBUG_THREADLIST)*/;
	m_configs[PaneId::Backtrace  ].Docked(Bottom).Layout(2,5).Actions(Hide,Hide,Show).Titles(LANG(CAPTION_BACKTRACE,"Trazado Inverso"))/*.MenuItem(mxID_DEBUG_BACKTRACE)*/;
	m_configs[PaneId::Inspections].Docked(Bottom).Layout(2,6).Actions(Hide,Hide,Show).Titles(LANG(CAPTION_INSPECTIONS,"Inspecciones"))/*.MenuItem(mxID_DEBUG_INSPECT)*/;
	
//	m_configs[PaneId::Tools      ].Docked(Bottom).Layout(4,6).Actions(None,Hide,Hide).Titles(,);
	
	m_configs[PaneId::Trees      ].Docked(Left  ).Layout(3,0).Actions(None,None,None).Titles(LANG(CAPTION_GROUPED_TREES,"Arboles")).SetAsContainer()/*.MenuItem(mxID_VIEW_LEFT_PANELS)*/;
	m_configs[PaneId::Project    ].Docked(Left  ).Layout(3,1).Actions(None,None,None).Titles(LANG(CAPTION_PROJECT_TREE,"Arbol de Archivos"),LANG(MAINW_AUTOHIDE_PROJECT,"Proyecto")).MenuItem(mxID_VIEW_PROJECT_TREE);
	m_configs[PaneId::Symbols    ].Docked(Left  ).Layout(3,2).Actions(None,None,None).Titles(LANG(CAPTION_SYMBOLS_TREE,"Arbol de Simbolos"),LANG(MAINW_AUTOHIDE_SYMBOLS,"Simbolos")).MenuItem(mxID_VIEW_SYMBOLS_TREE);
	m_configs[PaneId::Explorer   ].Docked(Left  ).Layout(3,3).Actions(None,None,None).Titles(LANG(CAPTION_EXPLORER_TREE,"Explorador de Archivos"),LANG(MAINW_AUTOHIDE_EXPLORER,"Explorador")).MenuItem(mxID_VIEW_EXPLORER_TREE);
	
	m_configs[PaneId::Beginners  ].Docked(Right ).Layout(3,0).Actions(None,None,Hide).Titles(LANG(MAINW_BEGGINERS_PANEL,"Panel de Asistencias")).MenuItem(mxID_VIEW_BEGINNER_PANEL);
//	m_configs[PaneId::Registers  ].Docked(Bottom).Layout(4,1).Actions(Hide,Hide,None).Titles(,);
	m_configs[PaneId::Minimap    ].Docked(Right ).Layout(5,9).Actions(None,None,Hide).Titles(LANG(MAINW_MINIMAP_PANEL,"Mini-mapa")).MenuItem(mxID_VIEW_MINIMAP).BestSize(100,100);
	
	// from config
	if (config->Init.autohide_panels) {
		m_configs[PaneId::Compiler   ].Autohide(true); 
		m_configs[PaneId::QuickHelp  ].Autohide(true); 
		m_configs[PaneId::DebugMsgs  ].Autohide(config->Debug.show_log_panel); 
		m_configs[PaneId::Threads    ].Autohide(config->Debug.show_thread_panel); 
		m_configs[PaneId::Backtrace  ].Autohide(true); 
		m_configs[PaneId::Inspections].Autohide(true); 
		m_configs[PaneId::Templates  ].Autohide(false); 
		m_configs[PaneId::Trees      ].Autohide(true); 
		m_configs[PaneId::Project    ].Autohide(true); 
		m_configs[PaneId::Symbols    ].Autohide(true); 
		m_configs[PaneId::Explorer   ].Autohide(true); 
		m_configs[PaneId::Registers  ].Autohide(false); 
		m_configs[PaneId::Beginners  ].Autohide(false); 
		m_configs[PaneId::Minimap    ].Autohide(false); 
		m_configs[PaneId::Tools      ].Autohide(false); 
	} else {
		for(int i=0;i<PaneId::Count;i++)
			m_configs[i].DontAutohide();
	}
	// extra debug panels
	m_configs[PaneId::Inspections].Docked(config->Debug.inspections_on_right?Right:Bottom);
	m_configs[PaneId::DebugMsgs  ].ActionDebug(config->Debug.show_log_panel?Show:None); 
	m_configs[PaneId::Threads    ].ActionDebug(config->Debug.show_thread_panel?Show:None); 
	PaneId::type id_left = (config->Init.left_panels && !config->Init.autohide_panels) ? PaneId::Trees : PaneId::Invalid;
	m_configs[PaneId::Project ].InsideOf(id_left);
	m_configs[PaneId::Symbols ].InsideOf(id_left);
	m_configs[PaneId::Explorer].InsideOf(id_left);
}

mxAUI::mxAUI (mxMainWindow *main_window) 
	: m_wxaui(*this), m_freeze_level(0), m_should_update(false),
	  m_fullscreen(false), m_debug(false)
{
	m_wxaui.SetManagedWindow(main_window);
	mxHidenPanel::main_window = main_window;
	PaneConfig::Init();
}

void mxAUI::Freeze ( ) {
	m_freeze_level++;
}

void mxAUI::Thaw ( ) {
	if (--m_freeze_level==0)
		if (m_should_update) {
			m_wxaui.Update();
			m_should_update = false;
		}
}

void mxAUI::Update ( ) {
	if (!m_freeze_level) m_wxaui.Update();
	else m_should_update = true;
}

void mxAUI::OnPaneClose (wxWindow * window) {
	for(int i=0;i<PaneId::Count;i++) {
		if (m_panes[i].window==window) {
			if (PaneConfig::Get(i).GetMenuId()!=wxID_ANY)
				_menu_item(PaneConfig::Get(i).GetMenuId())->Check(false);
			if (m_panes[i].hidden_helper)
				m_panes[i].hidden_helper->ProcessClose();
			if (i==PaneId::Trees) {
				_menu_item(PaneConfig::Get(PaneId::Project).GetMenuId())->Check(false);
				_menu_item(PaneConfig::Get(PaneId::Symbols).GetMenuId())->Check(false);
				_menu_item(PaneConfig::Get(PaneId::Explorer).GetMenuId())->Check(false);
			}
			return;
		}
	}
	int iaux = m_generic_panes.Find(window);
	if (iaux!=m_generic_panes.NotFound()) {
		m_wxaui.DetachPane(window);
		if (m_generic_panes[iaux].delete_on_close) window->Destroy();
		m_generic_panes.Remove(iaux);
	}
}

void mxAUI::AttachGenericPane(wxWindow *ctrl, wxString title, wxPoint position, wxSize size, bool handle_deletion) {
	wxAuiPaneInfo pane_info;
	pane_info.Float().CloseButton(true).MaximizeButton(true).Resizable(true).Caption(title).Show();
	if (position!=wxDefaultPosition) pane_info.FloatingPosition(position);
	if (size!=wxDefaultSize) pane_info.BestSize(size);
	m_wxaui.AddPane(ctrl,pane_info);
	mxAUI::Update();
	if (handle_deletion) m_generic_panes.Add(mxPaneInfo(ctrl).DeleteOnClose());
}

void mxAUI::Create(PaneId::type id, wxWindow * win) {
	const PaneConfig &cfg = PaneConfig::Get(id);
	mxPaneInfo &inf = m_panes[id];
	EXPECT_OR(inf.window==nullptr,return);
	inf.window = win;
	
	
	if (cfg.IsContained()) {
		inf.container = CreateContainer(cfg.GetContainerId());
		inf.container->Add(id,inf.window);
	} else {
		
		// add the control to the aui
		wxAuiPaneInfo pinfo;
		pinfo.Caption(cfg.GetCaption()).CloseButton(true).MaximizeButton(true).Hide().Position(cfg.GetOrder()).Layer(MAX_LAYER-cfg.GetLayer()).MaximizeButton(true);
		if (cfg.HaveSize()) pinfo.BestSize(cfg.GetSizeX(),cfg.GetSizeY());
		if (cfg.IsFloating()) pinfo.Float();
		else {
			pinfo.Dock();
			if      (cfg.IsBottom()) pinfo.Bottom();
			else if (cfg.IsLeft()) pinfo.Left();
			else if (cfg.IsRight()) pinfo.Right();
			else if (cfg.IsTop()) pinfo.Top();
		}
		m_wxaui.AddPane(inf.window, pinfo);
		
		// add the autohidding helper
		if (cfg.IsAutohiding()) {
			hp_pos pos = HP_BOTTOM; int layer = 0;
			if (cfg.IsLeft()) { pos = HP_LEFT; layer = 1; }
			if (cfg.IsRight()) { pos = HP_RIGHT; layer = 1; }
			inf.hidden_helper = new mxHidenPanel(m_wxaui.GetManagedWindow(),inf.window,pos,cfg.GetShortCaption());
			wxAuiPaneInfo pinfo;
			pinfo.CaptionVisible(false).Layer(MAX_LAYER-layer).Position(cfg.GetOrder()).Dock();
			if      (cfg.IsBottom()) pinfo.Bottom();
			else if (cfg.IsLeft()) pinfo.Left();
			else if (cfg.IsRight()) pinfo.Right();
			else if (cfg.IsTop()) pinfo.Top();
			if (cfg.ShowAutohideHandler()) pinfo.Show(); else pinfo.Hide();
			m_wxaui.AddPane(inf.hidden_helper, pinfo);
		}
	}	
}

void mxAUI::Hide (PaneId::type id) {
	if (m_panes[id].window) {
		if (m_panes[id].container) {
			if (m_panes[id].container->GetCurrent()==m_panes[id].window) {
				Hide(PaneConfig::Get(id).GetContainerId());
			}
		} else if (m_panes[id].hidden_helper) {
			m_panes[id].hidden_helper->Hide();
		} else {
			wxAuiPaneInfo &pane = m_wxaui.GetPane(m_panes[id].window);
			EXPECT_OR(pane.IsOk(),return);
			int menu_id = PaneConfig::Get(id).GetMenuId();
			if (menu_id!=wxID_ANY) _menu_item(menu_id)->Check(false);
			pane.Hide(); mxAUI::Update();
		}
	}
}

void mxAUI::Show (PaneId::type id, wxWindow * win, bool fixed) {
	if (!m_panes[id].window) Create(id,win);
	EXPECT(win==m_panes[id].window);
	Show(id,fixed);
}

void mxAUI::Show (PaneId::type id, bool fixed) {
	// contenido como tab en un notebook
	if (PaneConfig::Get(id).IsContained()) {
		Show(PaneConfig::Get(id).GetContainerId(),fixed);
		m_panes[id].container->Select(m_panes[id].window);
		return;
	} 
	// libre
	if (m_panes[id].hidden_helper) {
		if (fixed)
			m_panes[id].hidden_helper->ShowDock();
		else if (!m_panes[id].hidden_helper->IsVisible())
			m_panes[id].hidden_helper->ShowFloat(false);
	} else {
		wxAuiPaneInfo &pane = m_wxaui.GetPane(m_panes[id].window);
		EXPECT_OR(pane.IsOk(),return);
		int menu_id = PaneConfig::Get(id).GetMenuId();
		if (menu_id!=wxID_ANY) _menu_item(menu_id)->Check(true);
		pane.Show(); mxAUI::Update();
	}
}

bool mxAUI::ToggleFromMenu (PaneId::type id, wxWindow * win) {
	bool retval = false;
	if (win && !m_panes[id].window) Create(id,win);
	EXPECT((!win && m_panes[id].window) || win==m_panes[id].window);
	if (m_panes[id].hidden_helper) {
		m_wxaui.GetPane(m_panes[id].hidden_helper).Show();
		m_panes[id].hidden_helper->ShowDock();
		retval = true;
	} else {
		wxAuiPaneInfo &pane = m_wxaui.GetPane(m_panes[id].window);
		EXPECT_OR(pane.IsOk(),return false);
		int menu_id = PaneConfig::Get(id).GetMenuId();
		retval = !pane.IsShown();
		if (retval) pane.Show(); else pane.Hide(); 
		if (menu_id!=wxID_ANY) _menu_item(menu_id)->Check(retval);
		mxAUI::Update();
	}
	return retval;
}

void mxAUI::OnDebugStart ( ) {
	m_debug = true;
	if (!config->Debug.autohide_panels) return;
	for(int i=0;i<PaneId::Count;i++) { 
		if (!m_panes[i].window) return;
		if (PaneConfig::Get(i).ShowForDebug()) Show((PaneId::type)i,true);
		else if (PaneConfig::Get(i).HideForDebug()) Hide((PaneId::type)i);
	}
}

void mxAUI::OnDebugEnd ( ) {
	m_debug = false;
	if (!config->Debug.autohide_panels) return;
	for(int i=0;i<PaneId::Count;i++) { 
		if (!m_panes[i].window) return;
		if (m_fullscreen) {
			if (PaneConfig::Get(i).ShowForFullscreen()) Show((PaneId::type)i,true);
			else if (PaneConfig::Get(i).HideForFullscreen()) Hide((PaneId::type)i);
		} else {
			if (PaneConfig::Get(i).ShowForNormal()) Show((PaneId::type)i,true);
			else if (PaneConfig::Get(i).HideForNormal()) Hide((PaneId::type)i);
		}
	}
}

void mxAUI::OnFullScreenStart ( ) {
	m_fullscreen = true;
	if (!config->Init.autohide_panels_fs) return;
	for(int i=0;i<PaneId::Count;i++) { 
		if (!m_panes[i].window) return;
		if (m_debug) {
			if (PaneConfig::Get(i).ShowForDebug()) Show((PaneId::type)i,true);
			else if (PaneConfig::Get(i).HideForDebug()) Hide((PaneId::type)i);
		} else {
			if (PaneConfig::Get(i).ShowForFullscreen()) Show((PaneId::type)i,true);
			else if (PaneConfig::Get(i).HideForFullscreen()) Hide((PaneId::type)i);
		}
	}
}

void mxAUI::OnFullScreenEnd ( ) {
	m_fullscreen = false;
	if (!config->Init.autohide_panels_fs) return;
	for(int i=0;i<PaneId::Count;i++) { 
		if (!m_panes[i].window) return;
		if (m_debug) {
			if (PaneConfig::Get(i).ShowForDebug()) Show((PaneId::type)i,true);
			else if (PaneConfig::Get(i).HideForDebug()) Hide((PaneId::type)i);
		} else {
			if (PaneConfig::Get(i).ShowForNormal()) Show((PaneId::type)i,true);
			else if (PaneConfig::Get(i).HideForNormal()) Hide((PaneId::type)i);
		}
	}
}

void mxAUI::OnWelcomePanelShow ( ) {
	for(int i=0;i<PaneId::Count;i++) { 
		if (m_panes[i].window) {
			Hide((PaneId::type)i);
		}
	}
	
}

void mxAUI::OnWelcomePanelHide ( ) {
	if (m_fullscreen) OnFullScreenStart(); 
	else              OnFullScreenEnd();
	
}

void mxAUI::OnResize ( ) {
	for(int i=0;i<PaneId::Count;i++) { 
		if (m_panes[i].hidden_helper)
			m_panes[i].hidden_helper->ProcessParentResize();
	}
}

void mxAUI::RecreatePanes ( ) {
	mxAUIFreezeGuard guard(*this);
	bool was_visible[PaneId::Count];
	for(int i=0;i<PaneId::Count;i++)
		was_visible[i] = IsVisible((PaneId::type)i);
	// first, convert contained panes into free ones
	for(int i=0;i<PaneId::Count;i++) {
		if (m_panes[i].window && m_panes[i].container) {
			m_panes[i].container->Detach(m_panes[i].window);
			m_panes[i].window->Reparent(GetManagedWindow());
		}
	}
	// second, delete containers
	for(int i=0;i<PaneId::Count;i++) {
		PaneConfig &cfg = PaneConfig::Get(i);
		if (cfg.IsContainer() && m_panes[i].window) { 
			for(int id2=0;id2<PaneId::Count;id2++) 
				if (m_panes[id2].window && m_panes[id2].container==m_panes[i].window)
					was_visible[id2]=was_visible[i];
			m_wxaui.DetachPane(m_panes[i].window);
			m_panes[i].window->Destroy();
			m_panes[i].window=nullptr;
		}
	}
	// third, recreate all
	for(int i=0;i<PaneId::Count;i++) {
		PaneConfig &cfg = PaneConfig::Get(i);
		if (!cfg.IsContainer() && m_panes[i].window) {
			PaneId::type id = (PaneId::type)i;
			wxWindow *win = m_panes[i].window;
			if (m_panes[i].hidden_helper) {
				m_wxaui.DetachPane(m_panes[i].hidden_helper);
				m_panes[i].hidden_helper->Destroy();
				m_panes[i].hidden_helper = nullptr;
			}
			if (m_panes[i].container)
				m_panes[i].container = nullptr;
			else
				m_wxaui.DetachPane(m_panes[i].window);
			m_panes[i].window = nullptr;
			Create(id,win);
			if (cfg.IsContained()) {
				was_visible[i]|=was_visible[cfg.GetContainerId()];
				was_visible[cfg.GetContainerId()]|=was_visible[i];
			}
			if (was_visible[i]) Show(id,true); 
			else                Hide(id);
		}
	}
}

bool mxAUI::IsVisible (PaneId::type id) const {
	return m_panes[id].window && m_wxaui.GetPane(m_panes[id].window).IsShown();
}

mxAUIContainer * mxAUI::CreateContainer (PaneId::type id) {
	if (!m_panes[id].window)
		Create(id,new mxAUIContainer(m_wxaui.GetManagedWindow()));
	return (mxAUIContainer*)m_panes[id].window;
}

