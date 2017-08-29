#ifndef MXAUI_H
#define MXAUI_H
#include <wx/aui/aui.h>
#include <wx/textfile.h>
#include "SingleList.h"
#include "Cpp11.h"

class mxAUIContainer;

struct PaneId {
	enum type { 
		Compiler, 
		QuickHelp, 
		DebugMsgs, 
		Threads, 
		Backtrace, 
		Inspections, 
		Templates, 
		Trees, 
		Project, 
		Symbols, 
		Explorer, 
		Registers, 
		Beginners, 
		Minimap, 
		Tools, 
		Count,
		Container, // to mark containers as such
		Invalid=999 // equivalent to null
	};
};

struct PaneConfig {
private:
	enum side_t { Left, Bottom, Right, Top, Floating };
	enum action_t { None, Show, Hide };
	enum autohide_t { NoAutohide, AutohideNormal, AutohideNoHandler };
//	PaneId::type m_id;
	PaneId::type m_inside_of;
	wxString m_full_caption, m_short_caption, m_very_short_caption;
	autohide_t m_autohide;
	side_t m_side; ///< if mode==Docked
	int m_layer, m_order;
	int m_bs_x, m_bs_y;
	int m_menu_id;
	action_t m_normal;
	action_t m_debug;
	action_t m_fullscreen;
	
	// to construct
	PaneConfig() 
		: /*m_id(PaneId::Invalid), */m_inside_of(PaneId::Invalid), m_autohide(NoAutohide), m_side(Floating), 
		  m_layer(-1), m_order(-1), m_bs_x(-1), m_bs_y(-1), m_menu_id(wxID_ANY),
		  m_normal(None), m_debug(None), m_fullscreen(None)
	{
		
	}
	PaneConfig &SetAsContainer() { m_inside_of = PaneId::Container; return *this; }
	PaneConfig &InsideOf(PaneId::type id) { m_inside_of = id; return *this; }
	PaneConfig &Docked(side_t side) { m_side = side; return *this; }
	PaneConfig &Float() { m_side = Floating; return *this; }
	PaneConfig &Layout(int layer, int order) { m_layer = layer; m_order = order; return *this; }
	PaneConfig &DontAutohide() { m_autohide = NoAutohide; return *this; }
	PaneConfig &Autohide(bool show_handler) { m_autohide = (show_handler?AutohideNormal:AutohideNoHandler); return *this; }
	PaneConfig &BestSize(int x, int y) { m_bs_x = x; m_bs_y = y; return *this; }
	PaneConfig &MenuItem(int id) { m_menu_id = id; return *this; }
	PaneConfig &Titles(const wxString &full_caption, const wxString &short_caption="", const wxString &very_short_caption="") {
		m_short_caption = short_caption.IsEmpty()?full_caption:short_caption; 
		m_very_short_caption = very_short_caption.IsEmpty()?m_short_caption[0]:very_short_caption; 
		m_full_caption = full_caption; return *this;
	}
	PaneConfig &Actions(action_t normal, action_t fullscreen, action_t debug) {
		m_normal = normal; m_fullscreen = fullscreen; m_debug = debug; 
		return *this; 
	}
	PaneConfig &ActionNormal(action_t normal) { m_normal = normal; return *this; }
	PaneConfig &ActionFullscreen(action_t fullscreen) { m_fullscreen = fullscreen; return *this; }
	PaneConfig &ActionDebug(action_t debug) { m_debug = debug;  return *this; }
	
public:
	
	// to query
	bool IsAutohiding() const { return m_autohide!=NoAutohide; }
	bool ShowAutohideHandler() const { return m_autohide==AutohideNormal; }
	const wxString &GetCaption() const { return m_full_caption; }
	const wxString &GetShortCaption() const { return m_short_caption; }
	const wxString &GetVeryShortCaption() const { return m_very_short_caption; }
	bool IsContained() const { return m_inside_of!=PaneId::Invalid && m_inside_of!=PaneId::Container; }
	bool IsContainer() const { return m_inside_of==PaneId::Container; }
	PaneId::type GetContainerId() const { return m_inside_of; }
	int GetLayer() const { return m_layer; }
	int GetOrder() const { return m_order; }
	int GetSizeX() const { return m_bs_x; }
	int GetSizeY() const { return m_bs_y; }
	int HaveSize() const { return m_bs_y!=-1 && m_bs_x!=-1; }
	int IsLeft()   const { return m_side == Left; }
	int IsRight()  const { return m_side == Right; }
	int IsTop()    const { return m_side == Top; }
	int IsBottom() const { return m_side == Bottom; }
	int IsFloating() const { return m_side == Floating; }
	int GetMenuId() const { return m_menu_id; }
	bool ShowForNormal() const { return m_normal == Show; }
	bool HideForNormal() const { return m_normal == Hide; }
	bool ShowForFullscreen() const { return m_fullscreen == Show; }
	bool HideForFullscreen() const { return m_fullscreen == Hide; }
	bool ShowForDebug() const { return m_debug == Show; }
	bool HideForDebug() const { return m_debug == Hide; }
	
	static void Init();
	static PaneConfig m_configs[PaneId::Count];
	static PaneConfig &Get(PaneId::type id) { return m_configs[id]; }
	static PaneConfig &Get(int id) { return m_configs[(PaneId::type)id]; }
};

class mxHidenPanel;
class mxMainWindow;
class mxAUI;

class mxAUIFreezeGuard {
	mxAUIFreezeGuard &operator=(const mxAUIFreezeGuard &other);
protected:
	mxAUI &m_aui;
public:
	mxAUIFreezeGuard(mxAUI &aui);
	mxAUIFreezeGuard(const mxAUIFreezeGuard &other);
	~mxAUIFreezeGuard();
};

/// like wxAuiPaneInfo, but calls update on destructor (to be returned by mxAUI::AttachGenericPane so the caller can setup the pane)
class mxAUIPaneInfo : public mxAUIFreezeGuard {
	wxAuiPaneInfo &m_pane_info;
public:
	mxAUIPaneInfo(mxAUI &aui, wxWindow *ctrl);
	wxAuiPaneInfo *operator->();
};

class mxAUI : public wxAuiManager {
	wxAuiManager &m_wxaui;
	int m_freeze_level;
	bool m_should_update;
	bool m_fullscreen;
	bool m_debug;
	struct mxPaneInfo {
		wxWindow *window;
		mxHidenPanel *hidden_helper;
		mxAUIContainer *container;
		bool delete_on_close;
		mxPaneInfo() : window(nullptr), hidden_helper(nullptr), container(nullptr), delete_on_close(false) {}
		mxPaneInfo(wxWindow *win) : window(win), hidden_helper(nullptr), container(nullptr), delete_on_close(false) {}
		bool operator==(const mxPaneInfo &other) const { return window==other.window; }
		mxPaneInfo &DeleteOnClose() { delete_on_close = true; return *this; }
		mxPaneInfo &HiddenHelper(mxHidenPanel *helper) { hidden_helper = helper; return *this; }
	};
	mxPaneInfo m_panes[PaneId::Count];
	SingleList<mxPaneInfo> m_generic_panes;
public:
	mxAUI(mxMainWindow *main_window);
	mxAUIContainer *CreateContainer(PaneId::type id);
	void Create(PaneId::type id, wxWindow *win);
	void Show(PaneId::type id, wxWindow *win, bool fixed=false);
	void Show(PaneId::type id, bool fixed=false);
	void Hide(PaneId::type id);
	bool ToggleFromMenu(PaneId::type id, wxWindow *win=nullptr);
	bool OnPaneClose(wxWindow *window);
	void OnDebugStart();
	void OnDebugEnd();
	void OnFullScreenStart();
	void OnFullScreenEnd();
	void OnWelcomePanelShow();
	void OnWelcomePanelHide();
	void OnResize();
	bool OnKeyEscape(wxWindow *who);
	void Update();
//	void AttachKnownPane(int aui_id, wxWindow *ctrl);
	mxAUIPaneInfo AttachGenericPane(wxWindow *ctrl, wxString title, bool handle_deletion=true);
	void RecreatePanes();
	bool IsVisible(PaneId::type id) const;
private:
	void Freeze(); ///< use mxAUIFreezeGuard instead
	void Thaw(); ///< use mxAUIFreezeGuard instead
	friend class mxAUIFreezeGuard;
};


#endif

